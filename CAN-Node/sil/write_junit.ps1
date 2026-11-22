param(
  [Parameter(Mandatory=$true)][string[]]$LogPaths,
  [string[]]$ExtraLines=@(),
  [Parameter(Mandatory=$true)][string]$DefectDetailsPath,
  [Parameter(Mandatory=$true)][string]$OutPath
)
$ErrorActionPreference='Stop'
$defectSeverity=@{}
foreach($row in @(Import-Csv -LiteralPath $DefectDetailsPath)){$defectSeverity[$row.'defect ID']=$row.severity}
$records=@()
foreach($path in $LogPaths){
  if(Test-Path -LiteralPath $path){
    $source=[IO.Path]::GetFileNameWithoutExtension($path)
    $lineNumber=0
    foreach($line in @(Get-Content -LiteralPath $path)){
      ++$lineNumber
      if($line -match '^(PASS|FAIL|XPASS|XFAIL|BLOCKED)\s+'){
        $records+=[pscustomobject]@{Text=$line;Source=$source;Line=$lineNumber}
      }
    }
  }
}
$extraNumber=0
foreach($line in @($ExtraLines)){
  ++$extraNumber
  if($line -match '^(PASS|FAIL|XPASS|XFAIL|BLOCKED)\s+'){
    $records+=[pscustomobject]@{Text=$line;Source='run_sil';Line=$extraNumber}
  }
}
$driverXfail=@($records|Where-Object{
  $_.Text -match '^XFAIL\s+\S+\s+(DEF-[A-Z0-9-]+)' -and $defectSeverity[$Matches[1]] -eq 'DRIVER-IMPACTING'
})
$minorXfail=@($records|Where-Object{
  $_.Text -match '^XFAIL\s+\S+\s+(DEF-[A-Z0-9-]+)' -and $defectSeverity[$Matches[1]] -eq 'MINOR'
})
$failures=@($records|Where-Object{$_.Text -match '^(FAIL|XPASS)\s+'})+@($driverXfail)
$skipped=@($records|Where-Object{$_.Text -match '^BLOCKED\s+'})+@($minorXfail)

$settings=New-Object System.Xml.XmlWriterSettings
$settings.Indent=$true
$settings.Encoding=New-Object System.Text.UTF8Encoding($false)
$writer=[Xml.XmlWriter]::Create($OutPath,$settings)
try {
  $writer.WriteStartDocument()
  $writer.WriteStartElement('testsuite')
  $writer.WriteAttributeString('name','mfe26-sil')
  $writer.WriteAttributeString('tests',[string]$records.Count)
  $writer.WriteAttributeString('failures',[string]$failures.Count)
  $writer.WriteAttributeString('skipped',[string]$skipped.Count)
  foreach($record in $records){
    $line=$record.Text
    [void]($line -match '^(\S+)\s+(.*)$')
    $status=$Matches[1];$name=$Matches[2]
    $writer.WriteStartElement('testcase')
    $writer.WriteAttributeString('classname',"mfe26.sil.$($record.Source)")
    $writer.WriteAttributeString('name',$name)
    $writer.WriteAttributeString('file',$record.Source)
    $writer.WriteAttributeString('line',[string]$record.Line)
    if($status -eq 'FAIL' -or $status -eq 'XPASS'){
      $writer.WriteStartElement('failure');$writer.WriteAttributeString('message',$status);$writer.WriteString($line);$writer.WriteEndElement()
    } elseif($status -eq 'XFAIL') {
      [void]($line -match '^XFAIL\s+\S+\s+(DEF-[A-Z0-9-]+)');$severity=$defectSeverity[$Matches[1]]
      if($severity -eq 'DRIVER-IMPACTING'){
        $writer.WriteStartElement('failure');$writer.WriteAttributeString('message','DRIVER-IMPACTING XFAIL');$writer.WriteString($line);$writer.WriteEndElement()
      } elseif($severity -eq 'MINOR') {
        $writer.WriteStartElement('skipped');$writer.WriteAttributeString('message','MINOR WARNING');$writer.WriteString($line);$writer.WriteEndElement()
      } else {throw "Missing or invalid severity for JUnit XFAIL: $line"}
    } elseif($status -eq 'BLOCKED') {
      $writer.WriteStartElement('skipped');$writer.WriteAttributeString('message',$status);$writer.WriteString($line);$writer.WriteEndElement()
    }
    $writer.WriteEndElement()
  }
  $writer.WriteEndElement();$writer.WriteEndDocument()
} finally {$writer.Dispose()}
Write-Output "PASS JUNIT $OutPath"
