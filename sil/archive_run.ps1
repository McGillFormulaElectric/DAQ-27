param(
  [Parameter(Mandatory=$true)][string]$BuildDir,
  [Parameter(Mandatory=$true)][string]$TraceabilityPath,
  [Parameter(Mandatory=$true)][string]$DefectDetailsPath,
  [ValidateRange(1,1000)][int]$RetentionCount=10
)
$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..')
$commit=(& git -C $root rev-parse --short HEAD 2>$null).Trim()
$utc=(Get-Date).ToUniversalTime().ToString('yyyyMMddTHHmmssfffZ')
$runId="$utc-$commit"
$destination=Join-Path $BuildDir (Join-Path 'runs' $runId)
New-Item -ItemType Directory -Force -Path $destination|Out-Null
$authoritativeNames=[System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach($name in @('junit.xml','run-manifest.json','coverage-summary.json','coverage-production-summary.json','sil-summary.txt')){
  [void]$authoritativeNames.Add($name)
}
$artifacts=Get-ChildItem -LiteralPath $BuildDir -File -ErrorAction SilentlyContinue|Where-Object{
  $authoritativeNames.Contains($_.Name) -or
  $_.Name -match '^sil_.*tests\.(exe|log)$' -or
  $_.Name -match '^sil_(ads7066_model|adc|can|application)_build\.log$' -or
  $_.Name -eq 'ads7066_requirements_validation.log' -or
  $_.Name -match '\.gcov\.log$'
}
$artifacts|Copy-Item -Destination $destination
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'evidence\ads7066_figure_pin_traces.csv') -Destination $destination
$encoding=New-Object System.Text.UTF8Encoding($false)
$diff=@(& git -c core.safecrlf=false -C $root diff --binary --no-ext-diff --no-textconv 2>$null)
[IO.File]::WriteAllLines((Join-Path $destination 'source.diff'),[string[]]$diff,$encoding)
$status=@(& git -C $root status --short 2>$null)
[IO.File]::WriteAllLines((Join-Path $destination 'source-status.txt'),[string[]]$status,$encoding)
$untracked=@(& git -C $root ls-files --others --exclude-standard 2>$null)
foreach($relative in $untracked){
  $source=Join-Path $root $relative
  if(Test-Path -LiteralPath $source -PathType Leaf){
    $target=Join-Path $destination (Join-Path 'untracked-sources' $relative)
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target)|Out-Null
    Copy-Item -LiteralPath $source -Destination $target
  }
}
Copy-Item -LiteralPath $TraceabilityPath -Destination (Join-Path $destination 'traceability.csv')
Copy-Item -LiteralPath $DefectDetailsPath -Destination (Join-Path $destination 'defect_details.csv')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'ads7066_requirements.csv') -Destination (Join-Path $destination 'ads7066_requirements.csv')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'evidence\ads7066_evidence.csv') -Destination (Join-Path $destination 'ads7066_evidence.csv')
$runsRoot=Join-Path $BuildDir 'runs'
$resolvedRuns=(Resolve-Path -LiteralPath $runsRoot).Path
$pruned=0
$runPattern='^\d{8}T\d{9}Z-[0-9a-fA-F]+$'
$verifiedRuns=@(Get-ChildItem -LiteralPath $runsRoot -Directory | Where-Object{
  $_.Name -match $runPattern -and
  (Test-Path -LiteralPath (Join-Path $_.FullName 'traceability.csv') -PathType Leaf) -and
  @(Get-ChildItem -LiteralPath $_.FullName -File -ErrorAction SilentlyContinue | Where-Object{
    $_.Name -match '^sil_.*tests\.(exe|log)$'
  }).Count -gt 0 -and
  @(Get-ChildItem -LiteralPath $_.FullName -File -ErrorAction SilentlyContinue | Where-Object{
    $_.Name -match '^sil_(ads7066_model|adc|can|application)_build\.log$'
  }).Count -gt 0
})
$expired=@($verifiedRuns | Sort-Object Name -Descending | Select-Object -Skip $RetentionCount)
foreach($directory in $expired){
  if($directory.Parent.FullName -ne $resolvedRuns){throw "Refusing to prune archive outside $resolvedRuns"}
  Remove-Item -LiteralPath $directory.FullName -Recurse -Force
  ++$pruned
}
