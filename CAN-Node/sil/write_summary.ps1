param(
  [Parameter(Mandatory=$true)][string]$TraceabilityPath,
  [Parameter(Mandatory=$true)][string]$DefectDetailsPath,
  [Parameter(Mandatory=$true)][string[]]$LogPaths,
  [string[]]$ExtraLines=@(),
  [string[]]$ProcessFailures=@(),
  [Parameter(Mandatory=$true)][string]$OutPath
)
$ErrorActionPreference='Stop'

$rows=@(Import-Csv -LiteralPath $TraceabilityPath)
$defectRows=@(Import-Csv -LiteralPath $DefectDetailsPath)
$byId=@{}
foreach($row in $rows){$byId[$row.'SIL test ID']=$row}
$byDefect=@{}
foreach($row in $defectRows){
  $defectId=$row.'defect ID'
  if(-not $defectId){throw 'Defect metadata contains a row without a defect ID'}
  if($byDefect.ContainsKey($defectId)){throw "Duplicate defect metadata: $defectId"}
  $byDefect[$defectId]=$row
}

$lines=@()
foreach($path in $LogPaths){
  if(Test-Path -LiteralPath $path){$lines+=@(Get-Content -LiteralPath $path)}
}
$lines+=@($ExtraLines)

$passes=@($lines|Where-Object{$_ -match '^PASS\s+'})
$unexpected=@($lines|Where-Object{$_ -match '^(FAIL|XPASS)\s+' -or $_ -match 'CHECK failed:'}|Select-Object -Unique)
$xfails=@($lines|Where-Object{$_ -match '^XFAIL\s+'}|Select-Object -Unique)
$classifiedXfails=@()
foreach($line in $xfails){
  if($line -notmatch '^XFAIL\s+(\S+)\s+(\S+)'){
    throw "Malformed XFAIL record: $line"
  }
  $id=$Matches[1]
  $defect=$Matches[2]
  $detail=$byDefect[$defect]
  if(-not $detail){throw "Missing defect metadata for reproduced error: $defect"}
  if($detail.severity -notin @('DRIVER-IMPACTING','MINOR')){
    throw "Invalid severity for $defect; expected DRIVER-IMPACTING or MINOR"
  }
  foreach($field in @('severity rationale','cause','impact','expected correction','source location')){
    if(-not $detail.$field){throw "Missing '$field' metadata for reproduced error: $defect"}
  }
  $classifiedXfails+= [pscustomobject]@{
    Id=$id
    Defect=$defect
    Traceability=$byId[$id]
    Detail=$detail
  }
}
$driverImpacting=@($classifiedXfails|Where-Object{$_.Detail.severity -eq 'DRIVER-IMPACTING'})
$minor=@($classifiedXfails|Where-Object{$_.Detail.severity -eq 'MINOR'})

$report=[System.Collections.Generic.List[string]]::new()
[void]$report.Add('')
[void]$report.Add('================ SIL RUN SUMMARY ================')
if($unexpected.Count -gt 0 -or $ProcessFailures.Count -gt 0){
  [void]$report.Add('RESULT: FAILED - unexpected test failures require attention')
} elseif($driverImpacting.Count -gt 0){
  [void]$report.Add('RESULT: FAILED - driver-impacting errors were reproduced')
} elseif($minor.Count -gt 0){
  [void]$report.Add('RESULT: FUNCTIONAL CHECKS PASSED - advisory warnings remain')
} else {
  [void]$report.Add('RESULT: CLEAN - no failures, errors, or advisory warnings reported')
}
[void]$report.Add("PASS records: $($passes.Count) | Unexpected: $($unexpected.Count + $ProcessFailures.Count) | Errors: $($driverImpacting.Count) | Minor warnings: $($minor.Count)")
[void]$report.Add('Errors fail SIL/CI. Minor warnings remain visible but do not fail the ordinary development gate.')

if($unexpected.Count -gt 0 -or $ProcessFailures.Count -gt 0){
  [void]$report.Add('')
  [void]$report.Add('UNEXPECTED FAILURES')
  $allUnexpected=@($ProcessFailures)+@($unexpected)
  for($i=0;$i -lt $allUnexpected.Count;$i++){
    if($i -gt 0){[void]$report.Add('')}
    [void]$report.Add("- $($allUnexpected[$i])")
  }
}

function Add-ErrorSection {
  param([string]$Title,[object[]]$Items)
  if($Items.Count -eq 0){return}
  [void]$report.Add('')
  [void]$report.Add($Title)
  for($i=0;$i -lt $Items.Count;$i++){
    if($i -gt 0){[void]$report.Add('')}
    $item=$Items[$i]
    $row=$item.Traceability
    $detail=$item.Detail
    $name=if($row){$row.requirement}else{'Uncatalogued requirement'}
    [void]$report.Add("- $($item.Id) | $name | $($item.Defect)")
    [void]$report.Add("  Severity basis: $($detail.'severity rationale')")
    [void]$report.Add("  Cause: $($detail.cause)")
    [void]$report.Add("  Consequence: $($detail.impact)")
    [void]$report.Add("  Expected correction: $($detail.'expected correction')")
    [void]$report.Add("  Source/function: $($detail.'source location')")
  }
}

Add-ErrorSection -Title 'ERRORS' -Items $driverImpacting
Add-ErrorSection -Title 'MINOR WARNINGS' -Items $minor

[void]$report.Add('')
[void]$report.Add("Full details: $TraceabilityPath")
[void]$report.Add("Summary artifact: $OutPath")
[void]$report.Add('=================================================')
$report|Set-Content -Encoding UTF8 -LiteralPath $OutPath
$report
