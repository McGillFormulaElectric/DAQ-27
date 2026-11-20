param(
  [switch]$Strict,
  [string]$DbcPath = (Join-Path $PSScriptRoot 'evidence\controlling.dbc')
)
$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..')
$out = Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$defectRows=@(Import-Csv (Join-Path $PSScriptRoot 'defect_details.csv'))
$defectById=@{}
foreach($row in $defectRows){$defectById[$row.'defect ID']=$row}
function Write-SilStatus([string]$Path) {
  foreach($line in @(Get-Content -LiteralPath $Path)) {
    if($line -match '^XFAIL\s+(\S+)\s+(DEF-[A-Z0-9-]+)') {
      $severity=$defectById[$Matches[2]].severity
      if($severity -eq 'MINOR'){Write-Output "WARNING $($Matches[1]) $($Matches[2]) MINOR_XFAIL_advisory_non_blocking"}
      elseif($severity -eq 'DRIVER-IMPACTING'){Write-Output "ERROR $($Matches[1]) $($Matches[2]) DRIVER_IMPACTING_XFAIL_blocking"}
      else {Write-Output "ERROR $($Matches[1]) $($Matches[2]) missing_or_invalid_severity"}
    } elseif($line -match '^(PASS|FAIL|XPASS|BLOCKED|KNOWN_DEFECTS)' -or $line -match 'CHECK failed:') {Write-Output $line}
  }
}
function Invoke-SilCompiler([string[]]$Arguments,[string]$LogPath) {
  $previousPreference=$ErrorActionPreference
  try {
    # Native compilers write warnings to stderr. Keep them visible and archived
    # without allowing PowerShell to turn a successful warning into an exception.
    [IO.File]::WriteAllText($LogPath,'',(New-Object System.Text.UTF8Encoding($false)))
    $ErrorActionPreference='Continue'
    & g++ @Arguments 2>&1 | ForEach-Object {
      if($_ -is [System.Management.Automation.ErrorRecord]){$_.Exception.Message}else{$_}
    } | Tee-Object -FilePath $LogPath | Out-Host
    $compilerExit=$LASTEXITCODE
  } finally {
    $ErrorActionPreference=$previousPreference
  }
  return $compilerExit
}
function Invoke-SilExecutable([string]$Path,[string]$LogPath) {
  $startInfo=New-Object System.Diagnostics.ProcessStartInfo
  $startInfo.FileName=$Path
  $startInfo.UseShellExecute=$false
  $startInfo.CreateNoWindow=$true
  $startInfo.RedirectStandardOutput=$true
  $startInfo.RedirectStandardError=$true
  $process=New-Object System.Diagnostics.Process
  $process.StartInfo=$startInfo
  if(-not $process.Start()){throw "Failed to start SIL executable: $Path"}
  $stdout=$process.StandardOutput.ReadToEnd()
  $stderr=$process.StandardError.ReadToEnd()
  $process.WaitForExit()
  [IO.File]::WriteAllText($LogPath,($stdout+$stderr),(New-Object System.Text.UTF8Encoding($false)))
  return $process.ExitCode
}
function Stop-AdsPrerequisite([string]$Reason,[string]$LogPath) {
  if(-not (Test-Path -LiteralPath $LogPath)){[IO.File]::WriteAllText($LogPath,"FAIL ADS7066-PREREQUISITE $Reason`r`n",(New-Object System.Text.UTF8Encoding($false)))}
  elseif(-not (@(Get-Content -LiteralPath $LogPath)|Where-Object{$_ -match '^FAIL\s+'})){[IO.File]::AppendAllText($LogPath,"FAIL ADS7066-PREREQUISITE $Reason`r`n",(New-Object System.Text.UTF8Encoding($false)))}
  & (Join-Path $PSScriptRoot 'write_manifest.ps1') -Root $root -DbcPath $DbcPath -OutPath (Join-Path $out 'run-manifest.json') | Out-Null
  & (Join-Path $PSScriptRoot 'write_junit.ps1') -LogPaths @($LogPath) -DefectDetailsPath (Join-Path $PSScriptRoot 'defect_details.csv') -OutPath (Join-Path $out 'junit.xml') | Out-Null
  & (Join-Path $PSScriptRoot 'write_summary.ps1') -TraceabilityPath (Join-Path $PSScriptRoot 'traceability.csv') -DefectDetailsPath (Join-Path $PSScriptRoot 'defect_details.csv') -LogPaths @($LogPath) -OutPath (Join-Path $out 'sil-summary.txt') | Out-Null
  & (Join-Path $PSScriptRoot 'archive_run.ps1') -BuildDir $out -TraceabilityPath (Join-Path $PSScriptRoot 'traceability.csv') -DefectDetailsPath (Join-Path $PSScriptRoot 'defect_details.csv') | Out-Null
  throw "ADS7066 raw-model prerequisite failed: $Reason"
}
Get-ChildItem -LiteralPath $out -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '\.gc(da|no|ov)(\.log)?$' } | Remove-Item -Force
$coverageFlags=@('--coverage','-fprofile-abs-path')
$modelSources=@(
  (Join-Path $PSScriptRoot 'test_ads7066_model.cpp'),
  (Join-Path $PSScriptRoot 'ads7066_model.cpp'),
  (Join-Path $PSScriptRoot 'ads7066_bus.cpp'),
  (Join-Path $PSScriptRoot 'ads7066_fault_overlay.cpp')
)
$modelCompilerArgs=@('-std=c++17','-Wall','-Wextra','-Wpedantic','-g')+$coverageFlags+@("-I$PSScriptRoot")+$modelSources+@('-o',(Join-Path $out 'sil_ads7066_model_tests.exe'))
$modelBuildLog=Join-Path $out 'sil_ads7066_model_build.log'
$modelBuildExit=Invoke-SilCompiler -Arguments $modelCompilerArgs -LogPath $modelBuildLog
if($modelBuildExit -ne 0){Stop-AdsPrerequisite -Reason 'raw model conformance build failed' -LogPath $modelBuildLog}
$modelLog=Join-Path $out 'sil_ads7066_model_tests.log'
$modelExit=Invoke-SilExecutable -Path (Join-Path $out 'sil_ads7066_model_tests.exe') -LogPath $modelLog
Write-SilStatus $modelLog
if($modelExit -ne 0){Stop-AdsPrerequisite -Reason 'raw model conformance executable exited nonzero' -LogPath $modelLog}
$requirementsLog=Join-Path $out 'ads7066_requirements_validation.log'
try {
  $requirementsLines=@(& (Join-Path $PSScriptRoot 'validate_ads7066_requirements.ps1') -RequirementsPath (Join-Path $PSScriptRoot 'ads7066_requirements.csv') -RuntimeLogPath $modelLog -EvidenceIndexPath (Join-Path $PSScriptRoot 'evidence\ads7066_evidence.csv') -Strict:$Strict)
  [IO.File]::WriteAllLines($requirementsLog,$requirementsLines,(New-Object System.Text.UTF8Encoding($false)))
  $requirementsLines | Write-Output
} catch {
  [IO.File]::WriteAllText($requirementsLog,"FAIL ADS7066-REQUIREMENTS $($_.Exception.Message)`r`n",(New-Object System.Text.UTF8Encoding($false)))
  Stop-AdsPrerequisite -Reason "requirement/runtime join failed: $($_.Exception.Message)" -LogPath $requirementsLog
}

$sources = @(
  (Join-Path $PSScriptRoot 'test_adc.cpp'),
  (Join-Path $PSScriptRoot 'fakes\fake_hal.cpp'),
  (Join-Path $PSScriptRoot 'fakes\virtual_can.cpp'),
  (Join-Path $PSScriptRoot 'ads7066_model.cpp'),
  (Join-Path $PSScriptRoot 'ads7066_fault_overlay.cpp'),
  (Join-Path $root 'Drivers\MFE26-G4-Drivers\Src\HAL Drivers\IOPin.cpp'),
  (Join-Path $root 'Drivers\MFE26-G4-Drivers\Src\HAL Drivers\SPIDriver.cpp'),
  (Join-Path $root 'Drivers\MFE26-G4-Drivers\Src\HAL Drivers\SPIDevice.cpp'),
  (Join-Path $root 'Drivers\MFE26-G4-Drivers\Src\Device Drivers\ADS7066.cpp')
)
$includes = @(
  "-I$(Join-Path $PSScriptRoot 'fakes')",
  "-I$(Join-Path $root 'Drivers\MFE26-G4-Drivers\Inc\HAL Drivers')",
  "-I$(Join-Path $root 'Drivers\MFE26-G4-Drivers\Inc\Device Drivers')"
)
$adcCompilerArgs=@('-std=c++17','-Wall','-Wextra','-Wpedantic','-g')+$coverageFlags+$includes+$sources+@('-o',(Join-Path $out 'sil_adc_tests.exe'))
$adcBuildExit=Invoke-SilCompiler -Arguments $adcCompilerArgs -LogPath (Join-Path $out 'sil_adc_build.log')
if ($adcBuildExit -ne 0) { throw 'SIL build failed' }
$adcLog=Join-Path $out 'sil_adc_tests.log'
$adcExit=Invoke-SilExecutable -Path (Join-Path $out 'sil_adc_tests.exe') -LogPath $adcLog
Write-SilStatus $adcLog

$canSources = @(
  (Join-Path $PSScriptRoot 'test_can.cpp'),
  (Join-Path $PSScriptRoot 'fakes\fake_fdcan.cpp'),
  (Join-Path $PSScriptRoot 'fakes\virtual_can.cpp'),
  (Join-Path $root 'Drivers\MFE26-G4-Drivers\Src\HAL Drivers\CANMessage.cpp'),
  (Join-Path $root 'Drivers\MFE26-G4-Drivers\Src\HAL Drivers\CANDriver.cpp')
)
$canCompilerArgs=@('-std=c++17','-Wall','-Wextra','-Wpedantic','-g')+$coverageFlags+$includes+$canSources+@('-o',(Join-Path $out 'sil_can_tests.exe'))
$canBuildExit=Invoke-SilCompiler -Arguments $canCompilerArgs -LogPath (Join-Path $out 'sil_can_build.log')
if ($canBuildExit -ne 0) { throw 'CAN SIL build failed' }
$canLog=Join-Path $out 'sil_can_tests.log'
$canExit=Invoke-SilExecutable -Path (Join-Path $out 'sil_can_tests.exe') -LogPath $canLog
Write-SilStatus $canLog

$appSources = @(
  (Join-Path $PSScriptRoot 'test_application.cpp'),
  (Join-Path $PSScriptRoot 'fakes\fake_hal.cpp'),
  (Join-Path $PSScriptRoot 'fakes\virtual_can.cpp'),
  (Join-Path $PSScriptRoot 'ads7066_model.cpp'),
  (Join-Path $PSScriptRoot 'ads7066_fault_overlay.cpp'),
  (Join-Path $root 'Core\Src\BoardManager.cpp'),
  (Join-Path $root 'Core\Src\Wrapper.cpp'),
  (Join-Path $root 'Drivers\MFE26-G4-Drivers\Src\HAL Drivers\IOPin.cpp'),
  (Join-Path $root 'Drivers\MFE26-G4-Drivers\Src\HAL Drivers\SPIDriver.cpp'),
  (Join-Path $root 'Drivers\MFE26-G4-Drivers\Src\HAL Drivers\SPIDevice.cpp'),
  (Join-Path $root 'Drivers\MFE26-G4-Drivers\Src\HAL Drivers\CANMessage.cpp'),
  (Join-Path $root 'Drivers\MFE26-G4-Drivers\Src\HAL Drivers\CANDriver.cpp'),
  (Join-Path $root 'Drivers\MFE26-G4-Drivers\Src\Device Drivers\ADS7066.cpp')
)
$appIncludes = $includes + @("-I$(Join-Path $root 'Core\Inc')")
$appCompilerArgs=@('-std=c++17','-Wall','-Wextra','-Wpedantic','-g')+$coverageFlags+$appIncludes+$appSources+@('-o',(Join-Path $out 'sil_application_tests.exe'))
$appBuildExit=Invoke-SilCompiler -Arguments $appCompilerArgs -LogPath (Join-Path $out 'sil_application_build.log')
if ($appBuildExit -ne 0) { throw 'Application SIL build failed' }
$appLog=Join-Path $out 'sil_application_tests.log'
$appExit=Invoke-SilExecutable -Path (Join-Path $out 'sil_application_tests.exe') -LogPath $appLog
Write-SilStatus $appLog

$dbcAvailable=Test-Path -LiteralPath $DbcPath
$dbcLines = if ($dbcAvailable) { @(& (Join-Path $PSScriptRoot 'verify_dbc.ps1') -DbcPath $DbcPath) }
            else { @('BLOCKED SIL-CAN-006 version_locked_DBC_path_must_be_supplied_to_verify_dbc.ps1') }
$mainLines=@(& (Join-Path $PSScriptRoot 'verify_main_loop.ps1') `
  -MainPath (Join-Path $root 'Core\Src\main.c'))
$adcSpiLines=@(& (Join-Path $PSScriptRoot 'verify_adc_spi.ps1') -Root $root)
$coverageLines=@(& (Join-Path $PSScriptRoot 'collect_coverage.ps1') -BuildDir $out)
$qualityLines=@(
  'PASS QUALITY compiler_warnings_are_visible_advisories_and_compiler_errors_are_blocking',
  'BLOCKED QUALITY-SANITIZERS sanitizer_campaign_not_executed_by_this_host_run',
  'BLOCKED QUALITY-STATIC clang_tidy_and_cppcheck_campaign_not_executed_by_this_host_run',
  'BLOCKED QUALITY-MUTATION mutation_campaign_not_executed_by_this_host_run',
  'BLOCKED QUALITY-DUAL-COMPILER clang_dual_build_not_executed_by_this_host_run',
  'BLOCKED QUALITY-CROSS-BUILD ARM_target_cross_build_not_executed_by_this_host_run'
)
$dbcLines | Write-Output
$mainLines | Write-Output
$adcSpiLines | Write-Output
$coverageLines | Write-Output
$qualityLines | Write-Output
& (Join-Path $PSScriptRoot 'write_manifest.ps1') -Root $root -DbcPath $DbcPath -OutPath (Join-Path $out 'run-manifest.json')

$emitted=@()
$emitted += @(Get-Content $modelLog)
$emitted += @($requirementsLines)
$emitted += @(Get-Content $adcLog)
$emitted += @(Get-Content $canLog)
$emitted += @(Get-Content $appLog)
$emitted += @($dbcLines)
$emitted += @($mainLines)
$emitted += @($adcSpiLines)
$emitted += @($coverageLines)
$emitted += @($qualityLines)
$rows=@(Import-Csv (Join-Path $PSScriptRoot 'traceability.csv'))
$adsRows=@(Import-Csv (Join-Path $PSScriptRoot 'ads7066_requirements.csv'))
$runtimeXfails=@($emitted|Where-Object{$_ -match '^XFAIL\s+\S+\s+DEF-[A-Z0-9-]+'}|Select-Object -Unique)
$blockingXfails=@($runtimeXfails|Where-Object{
  $_ -match '^XFAIL\s+\S+\s+(DEF-[A-Z0-9-]+)' -and $defectById[$Matches[1]].severity -eq 'DRIVER-IMPACTING'
})
$runtimeFailures=@()
if($modelExit -ne 0){$runtimeFailures+='ADS7066 model conformance executable exited nonzero'}
if($adcExit -ne 0){$runtimeFailures+='ADC SIL executable exited nonzero'}
if($canExit -ne 0){$runtimeFailures+='CAN SIL executable exited nonzero'}
if($appExit -ne 0){$runtimeFailures+='Application SIL executable exited nonzero'}
$processLines=@($runtimeFailures|ForEach-Object{"FAIL PROCESS $($_ -replace '\s+','_')"})
$summaryPath=Join-Path $out 'sil-summary.txt'
$junitPath=Join-Path $out 'junit.xml'
& (Join-Path $PSScriptRoot 'write_junit.ps1') `
  -LogPaths @($modelLog,$requirementsLog,$adcLog,$canLog,$appLog) `
  -ExtraLines @($dbcLines+$mainLines+$adcSpiLines+$coverageLines+$qualityLines+$processLines) `
  -DefectDetailsPath (Join-Path $PSScriptRoot 'defect_details.csv') `
  -OutPath $junitPath
& (Join-Path $PSScriptRoot 'write_summary.ps1') `
  -TraceabilityPath (Join-Path $PSScriptRoot 'traceability.csv') `
  -DefectDetailsPath (Join-Path $PSScriptRoot 'defect_details.csv') `
  -LogPaths @($modelLog,$requirementsLog,$adcLog,$canLog,$appLog) `
  -ExtraLines @($dbcLines+$mainLines+$adcSpiLines+$coverageLines+$qualityLines) `
  -ProcessFailures $runtimeFailures `
  -OutPath $summaryPath
& (Join-Path $PSScriptRoot 'archive_run.ps1') `
  -BuildDir $out `
  -TraceabilityPath (Join-Path $PSScriptRoot 'traceability.csv') `
  -DefectDetailsPath (Join-Path $PSScriptRoot 'defect_details.csv')

if($runtimeFailures.Count -gt 0){throw "SIL test executable failure; see $summaryPath"}
if(-not $Strict -and $blockingXfails.Count -gt 0){throw "SIL reproduced $($blockingXfails.Count) DRIVER-IMPACTING error(s); see $summaryPath"}

if($Strict) {
  $expectedXfails=[System.Collections.Generic.HashSet[string]]::new()
  foreach($row in $rows) {
    foreach($match in [regex]::Matches($row.'issue/deviation','DEF-[A-Z0-9-]+')) {
      [void]$expectedXfails.Add("$($row.'SIL test ID')|$($match.Value)")
    }
  }
  $actualXfails=[System.Collections.Generic.HashSet[string]]::new()
  foreach($line in $emitted) {
    if($line -match '^XFAIL\s+(\S+)\s+(DEF-[A-Z0-9-]+)') {
      $pair="$($Matches[1])|$($Matches[2])"
      if(-not $actualXfails.Add($pair)) {
        throw "Strict SIL gate: duplicate emitted XFAIL pair: $pair"
      }
    }
  }
  $xfailDrift=@(Compare-Object @($expectedXfails) @($actualXfails))
  if($xfailDrift) {
    $detail=($xfailDrift | ForEach-Object { if($_.SideIndicator -eq '<='){"missing:$($_.InputObject)"}else{"unexpected:$($_.InputObject)"} }) -join ', '
    throw "Strict SIL gate: XFAIL traceability drift: $detail"
  }
  $reportedKnown=0
  foreach($line in $emitted){if($line -match '^KNOWN_DEFECTS\s+(\d+)'){$reportedKnown+=[int]$Matches[1]}}
  if($reportedKnown -ne $actualXfails.Count){
    throw "Strict SIL gate: KNOWN_DEFECTS totals=$reportedKnown but unique XFAIL pairs=$($actualXfails.Count)"
  }

  if(-not $dbcAvailable){throw 'Strict SIL gate: controlling DBC was not executed'}
  $actualBlockedIds=[System.Collections.Generic.HashSet[string]]::new()
  $expectedBlockedIds=[System.Collections.Generic.HashSet[string]]::new()
  foreach($id in @($rows|Where-Object{$_.'actual result' -like 'BLOCKED*'}|ForEach-Object{$_.'SIL test ID'})){[void]$expectedBlockedIds.Add($id)}
  foreach($id in @($adsRows|Where-Object{$_.disposition -eq 'HIL_REQUIRED'}|ForEach-Object{$_.test_id})){[void]$expectedBlockedIds.Add($id)}
  $adsOpen=@($adsRows|Where-Object{$_.classification -in @('DIGITAL_MODEL','DIGITAL_INJECTION') -and $_.disposition -ne 'AUTOMATED'})
  if($adsOpen.Count -gt 0){[void]$expectedBlockedIds.Add('ADS-REQUIREMENTS')}
  foreach($line in $emitted) {
    if($line -match '^BLOCKED\s+(\S+)') {
      $id=$Matches[1]
      [void]$actualBlockedIds.Add($id)
      if(-not $expectedBlockedIds.Contains($id)){throw "Strict SIL gate: emitted BLOCKED $id has no matching traceability or ADS requirement row"}
    }
  }
  $blockedDrift=@(Compare-Object @($expectedBlockedIds) @($actualBlockedIds))
  if($blockedDrift){
    $detail=($blockedDrift|ForEach-Object{if($_.SideIndicator -eq '<='){"missing:$($_.InputObject)"}else{"unexpected:$($_.InputObject)"}})-join ', '
    throw "Strict SIL gate: BLOCKED traceability drift: $detail"
  }
  $open=$rows | Where-Object {
    if($_.'actual result' -ne 'AUTOMATED'){return $true}
    $prefix="$($_.'SIL test ID')|"
    foreach($pair in $actualXfails){if($pair.StartsWith($prefix)){return $true}}
    return $false
  }
  if($open){
    $summary=($open | ForEach-Object { "$($_.'SIL test ID')=$($_.'actual result')" }) -join ', '
    throw "Strict SIL gate is red: $summary"
  }
  if($adsOpen.Count -gt 0 -or @($adsRows|Where-Object{$_.disposition -eq 'HIL_REQUIRED'}).Count -gt 0){
    $adsSummary=(@($adsOpen|ForEach-Object{"$($_.requirement_id)=$($_.disposition)"})+@($adsRows|Where-Object{$_.disposition -eq 'HIL_REQUIRED'}|ForEach-Object{"$($_.requirement_id)=HIL_REQUIRED"})) -join ', '
    throw "Strict ADS7066 gate is red: $adsSummary"
  }
}
