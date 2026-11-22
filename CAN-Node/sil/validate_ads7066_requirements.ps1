param(
  [Parameter(Mandatory=$true)][string]$RequirementsPath,
  [Parameter(Mandatory=$true)][string]$RuntimeLogPath,
  [Parameter(Mandatory=$true)][string]$EvidenceIndexPath,
  [switch]$Strict
)
$ErrorActionPreference='Stop'
$rows=@(Import-Csv -LiteralPath $RequirementsPath)
if($rows.Count -eq 0){throw 'ADS7066 requirement matrix is empty'}
$requiredColumns=@('requirement_id','classification','atomic_requirement','evidence_id','evidence_locator','evidence_sha256','implementation','test_id','disposition')
foreach($column in $requiredColumns){if($rows[0].PSObject.Properties.Name -notcontains $column){throw "ADS7066 requirement matrix missing column: $column"}}
$evidenceRows=@(Import-Csv -LiteralPath $EvidenceIndexPath)
$evidenceById=@{}
foreach($evidence in $evidenceRows){if(-not $evidence.evidence_id -or $evidenceById.ContainsKey($evidence.evidence_id)){throw "Missing or duplicate ADS7066 evidence_id: $($evidence.evidence_id)"};$evidenceById[$evidence.evidence_id]=$evidence}
$silRoot=Split-Path -Parent $RequirementsPath
$evidenceFiles=@{
  'ADS7066-REV-C'=(Join-Path $silRoot 'evidence\ADS7066-RevC-SBAS928C.pdf')
  'ADS7066-CRC-SBAA456'=(Join-Path $silRoot 'evidence\SBAA456-ADS7066-CRC.pdf')
  'SIL-POLICY'=(Join-Path $silRoot 'sil_instructions.md')
}
foreach($evidenceId in $evidenceById.Keys){
  if(-not $evidenceFiles.ContainsKey($evidenceId)){throw "ADS7066 evidence index has no archived-file mapping: $evidenceId"}
  $path=$evidenceFiles[$evidenceId]
  if(-not (Test-Path -LiteralPath $path -PathType Leaf)){throw "ADS7066 evidence file is absent: $path"}
  $actualHash=(Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash
  $actualBytes=(Get-Item -LiteralPath $path).Length
  if($actualHash -ne $evidenceById[$evidenceId].sha256 -or $actualBytes -ne [int64]$evidenceById[$evidenceId].bytes){throw "ADS7066 evidence archive drift: $evidenceId"}
}
$ids=[System.Collections.Generic.HashSet[string]]::new()
foreach($row in $rows){
  if(-not $row.requirement_id -or -not $ids.Add($row.requirement_id)){throw "Missing or duplicate ADS7066 requirement_id: $($row.requirement_id)"}
  if($row.classification -notin @('DIGITAL_MODEL','DIGITAL_INJECTION','HIL_REQUIRED')){throw "Invalid ADS7066 classification for $($row.requirement_id): $($row.classification)"}
  if($row.disposition -notin @('AUTOMATED','NOT_IMPLEMENTED','HIL_REQUIRED')){throw "Invalid ADS7066 disposition for $($row.requirement_id): $($row.disposition)"}
  if(($row.classification -eq 'HIL_REQUIRED') -ne ($row.disposition -eq 'HIL_REQUIRED')){throw "ADS7066 HIL classification/disposition mismatch: $($row.requirement_id)"}
  if(-not $row.evidence_id -or -not $row.evidence_locator -or -not $row.evidence_sha256 -or -not $row.implementation){throw "Incomplete ADS7066 trace row: $($row.requirement_id)"}
  if(-not $evidenceById.ContainsKey($row.evidence_id)){throw "ADS7066 row cites unknown evidence: $($row.requirement_id) -> $($row.evidence_id)"}
  if($row.evidence_sha256 -ne $evidenceById[$row.evidence_id].sha256){throw "ADS7066 row evidence hash disagrees with index: $($row.requirement_id)"}
}
$runtime=@(Get-Content -LiteralPath $RuntimeLogPath)
$passCounts=@{}
$failIds=[System.Collections.Generic.HashSet[string]]::new()
foreach($line in $runtime){
  if($line -match '^PASS\s+(\S+)'){if(-not $passCounts.ContainsKey($Matches[1])){$passCounts[$Matches[1]]=0};$passCounts[$Matches[1]]++}
  elseif($line -match '^FAIL\s+(\S+)'){[void]$failIds.Add($Matches[1])}
}
$automated=@($rows|Where-Object{$_.disposition -eq 'AUTOMATED'})
$automatedTests=[System.Collections.Generic.HashSet[string]]::new()
$hilTests=[System.Collections.Generic.HashSet[string]]::new()
foreach($row in @($rows|Where-Object{$_.disposition -eq 'HIL_REQUIRED'})){if(-not $row.test_id -or -not $hilTests.Add($row.test_id)){throw "HIL ADS7066 test_id is missing or not unique: $($row.test_id)"}}
foreach($row in $automated){
  if(-not $row.test_id){throw "AUTOMATED ADS7066 row has no test_id: $($row.requirement_id)"}
  if(-not $automatedTests.Add($row.test_id)){throw "AUTOMATED ADS7066 test_id is not atomic/unique: $($row.test_id)"}
  if(-not $passCounts.ContainsKey($row.test_id) -or $passCounts[$row.test_id] -ne 1){throw "AUTOMATED ADS7066 row lacks exactly one runtime PASS: $($row.requirement_id) -> $($row.test_id)"}
  if($failIds.Contains($row.test_id)){throw "AUTOMATED ADS7066 row also emitted FAIL: $($row.requirement_id) -> $($row.test_id)"}
}
foreach($testId in @($passCounts.Keys|Where-Object{$_ -like 'ADS-DIG-*'})){
  if(-not $automatedTests.Contains($testId)){throw "Unmapped ADS7066 atomic PASS is not an AUTOMATED matrix row: $testId"}
}
$openDigital=@($rows|Where-Object{$_.classification -in @('DIGITAL_MODEL','DIGITAL_INJECTION') -and $_.disposition -ne 'AUTOMATED'})
$hil=@($rows|Where-Object{$_.classification -eq 'HIL_REQUIRED'})
foreach($row in $hil){Write-Output "BLOCKED $($row.test_id) HIL_REQUIRED $($row.requirement_id)"}
if($openDigital.Count -gt 0){Write-Output "BLOCKED ADS-REQUIREMENTS open_digital=$($openDigital.Count) automated=$($automated.Count) hil_required=$($hil.Count)"}
else{Write-Output "PASS ADS-REQUIREMENTS exact_runtime_join automated=$($automated.Count) hil_required=$($hil.Count)"}
