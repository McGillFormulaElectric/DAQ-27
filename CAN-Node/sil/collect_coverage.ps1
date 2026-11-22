param([Parameter(Mandatory=$true)][string]$BuildDir)
$ErrorActionPreference='Stop'

function Convert-CoveragePercent([string]$value) {
  [double]::Parse(($value -replace ',','.'),[Globalization.CultureInfo]::InvariantCulture)
}

# Each row names one compiler artifact. Metrics from different executables are
# intentionally never merged or independently maximized.
$specs=@(
  [pscustomobject]@{scope='raw-model';target='sil_ads7066_model_tests';artifact='sil_ads7066_model_tests-ads7066_model.gcno';source='ads7066_model.cpp';line=98.0;function=100.0;branch=95.0},
  [pscustomobject]@{scope='raw-model';target='sil_ads7066_model_tests';artifact='sil_ads7066_model_tests-ads7066_bus.gcno';source='ads7066_bus.cpp';line=98.0;function=100.0;branch=95.0},
  [pscustomobject]@{scope='raw-model';target='sil_ads7066_model_tests';artifact='sil_ads7066_model_tests-ads7066_fault_overlay.gcno';source='ads7066_fault_overlay.cpp';line=98.0;function=100.0;branch=95.0},
  [pscustomobject]@{scope='adc-driver';target='sil_adc_tests';artifact='sil_adc_tests-ADS7066.gcno';source='ADS7066.cpp';line=95.0;function=100.0;branch=90.0},
  [pscustomobject]@{scope='adc-driver';target='sil_adc_tests';artifact='sil_adc_tests-SPIDriver.gcno';source='SPIDriver.cpp';line=95.0;function=100.0;branch=90.0},
  [pscustomobject]@{scope='adc-driver';target='sil_adc_tests';artifact='sil_adc_tests-SPIDevice.gcno';source='SPIDevice.cpp';line=95.0;function=100.0;branch=90.0},
  [pscustomobject]@{scope='adc-driver';target='sil_adc_tests';artifact='sil_adc_tests-IOPin.gcno';source='IOPin.cpp';line=95.0;function=100.0;branch=90.0}
)

$results=@()
Push-Location $BuildDir
try {
  foreach($spec in $specs){
    if(-not(Test-Path -LiteralPath $spec.artifact)){throw "Required coverage artifact missing: $($spec.artifact)"}
    $previousErrorAction=$ErrorActionPreference
    $ErrorActionPreference='Continue'
    try {$output=@(& gcov -j -b -c $spec.artifact 2>&1)}
    finally {$ErrorActionPreference=$previousErrorAction}
    $output|Set-Content -Encoding UTF8 -LiteralPath (($spec.artifact -replace '\.gcno$','')+'.gcov.log')

    $jsonPath=($spec.artifact -replace '\.gcno$','.gcov.json.gz')
    if(-not(Test-Path -LiteralPath $jsonPath)){throw "gcov JSON missing for $($spec.artifact)"}
    $fileStream=[IO.File]::OpenRead((Resolve-Path -LiteralPath $jsonPath))
    try{
      $gzip=[IO.Compression.GZipStream]::new($fileStream,[IO.Compression.CompressionMode]::Decompress)
      try{$reader=[IO.StreamReader]::new($gzip);try{$json=$reader.ReadToEnd()|ConvertFrom-Json}finally{$reader.Dispose()}}finally{$gzip.Dispose()}
    } finally {$fileStream.Dispose()}
    $jsonFile=@($json.files|Where-Object{[IO.Path]::GetFileName($_.file)-ceq$spec.source})
    if($jsonFile.Count-ne1){throw "gcov JSON did not uniquely report $($spec.source) from $($spec.artifact)"}
    $functionTotal=@($jsonFile[0].functions).Count
    $functionExecuted=@($jsonFile[0].functions|Where-Object{$_.execution_count-gt0}).Count
    $matched=$null;$isExpected=$false
    foreach($line in $output){
      if($line -match "^File '(.+)'$"){
        $file=$Matches[1];$isExpected=([IO.Path]::GetFileName($file) -ceq $spec.source)
      }
      elseif($isExpected-and$line -match '^Lines executed:([0-9]+(?:[.,][0-9]+)?)% of ([0-9]+)'){$linePct=Convert-CoveragePercent $Matches[1];$lineCount=[int]$Matches[2]}
      elseif($isExpected-and$line -match '^Branches executed:([0-9]+(?:[.,][0-9]+)?)% of ([0-9]+)'){$branchCount=[int]$Matches[2]}
      elseif($isExpected-and$line -match '^Taken at least once:([0-9]+(?:[.,][0-9]+)?)% of ([0-9]+)'){
        $branchPct=Convert-CoveragePercent $Matches[1]
        $functionPct=if($functionTotal){100.0*$functionExecuted/$functionTotal}else{100.0}
        $matched=[pscustomobject][ordered]@{
          scope=$spec.scope;target=$spec.target;artifact=$spec.artifact;file=$file
          lines_percent=$linePct;lines=$lineCount;functions_percent=$functionPct
          functions_executed=$functionExecuted;functions=$functionTotal
          branches_taken_percent=$branchPct;branches=$branchCount
          threshold_lines=$spec.line;threshold_functions=$spec.function;threshold_branches=$spec.branch
          passed=($linePct-ge$spec.line-and$functionPct-ge$spec.function-and$branchPct-ge$spec.branch)
        }
        $isExpected=$false
      }
      elseif($isExpected-and$line -eq 'No branches'){
        $functionPct=if($functionTotal){100.0*$functionExecuted/$functionTotal}else{100.0}
        $matched=[pscustomobject][ordered]@{
          scope=$spec.scope;target=$spec.target;artifact=$spec.artifact;file=$file
          lines_percent=$linePct;lines=$lineCount;functions_percent=$functionPct
          functions_executed=$functionExecuted;functions=$functionTotal
          branches_taken_percent=100.0;branches=0
          threshold_lines=$spec.line;threshold_functions=$spec.function;threshold_branches=$spec.branch
          passed=($linePct-ge$spec.line-and$functionPct-ge$spec.function)
        }
        $isExpected=$false
      }
    }
    if($null-eq$matched){throw "gcov did not report $($spec.source) from $($spec.artifact)"}
    $results+=$matched
  }
} finally {Pop-Location}

$results|ConvertTo-Json -Depth 4|Set-Content -Encoding UTF8 -LiteralPath (Join-Path $BuildDir 'coverage-summary.json')
@($results|Where-Object{$_.scope-eq'adc-driver'})|ConvertTo-Json -Depth 4|Set-Content -Encoding UTF8 -LiteralPath (Join-Path $BuildDir 'coverage-production-summary.json')
foreach($row in $results){
  Write-Output "COVERAGE scope=$($row.scope) target=$($row.target) artifact=$($row.artifact) line=$($row.lines_percent) function=$([math]::Round($row.functions_percent,2)) branch_taken=$($row.branches_taken_percent) pass=$($row.passed)"
}
foreach($scope in @('raw-model','adc-driver')){
  $failed=@($results|Where-Object{$_.scope-eq$scope-and-not$_.passed})
  if($failed.Count){Write-Output "BLOCKED QUALITY-COVERAGE-$($scope.ToUpper()) explicit_target_thresholds_not_met artifacts=$($failed.artifact -join ',')"}
  else{Write-Output "PASS QUALITY-COVERAGE-$($scope.ToUpper()) explicit_target_thresholds_met"}
}
