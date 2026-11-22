param([Parameter(Mandatory=$true)][string]$DbcPath)
$ErrorActionPreference='Stop'
$expectedHash='2679E13ECC0B9E7A0EBAED230AF75F7C4E75B371F07D4969D3E87A795494696B'
$hash=(Get-FileHash -Algorithm SHA256 -LiteralPath $DbcPath).Hash
if($hash -ne $expectedHash){throw "SIL-CAN-006 DBC hash mismatch: $hash"}
$messages=@{}; $current=$null
foreach($line in Get-Content -LiteralPath $DbcPath){
  if($line -match '^BO_\s+(\d+)\s+([^:]+):\s+(\d+)\s+(\S+)'){
    $current=[int]$Matches[1]; $messages[$current]=[ordered]@{name=$Matches[2];dlc=[int]$Matches[3];node=$Matches[4];signals=@()}
  } elseif($current -ne $null -and $line -match '^\s+SG_\s+(\S+)\s+:\s+(\d+)\|(\d+)@([01])([+-])\s+\(([^,]+),([^\)]+)\)\s+\[([^|]+)\|([^\]]+)\]\s+"([^"]*)"'){
    $messages[$current].signals += "$($Matches[1]):$($Matches[2])|$($Matches[3])@$($Matches[4])$($Matches[5]):$($Matches[6]):$($Matches[7]):$($Matches[8]):$($Matches[9]):$($Matches[10])"
  }
}
$front=@($messages.GetEnumerator()|?{$_.Value.node -eq 'FrontNode'}|% Key|Sort-Object)
$rear=@($messages.GetEnumerator()|?{$_.Value.node -eq 'RearCAN'}|% Key|Sort-Object)
$inScope=@(200,210,300,700,800,500,600,720,810)|Sort-Object
$selected=@($front+$rear|?{$_ -notin @(100,110,400)}|Sort-Object)
if(Compare-Object $inScope $selected){throw "SIL-CAN-006 transmitter/ownership selection mismatch: $selected"}
$required=@{
  200=@('Temp1:0|12@1+:0.1:-40:-40:180:degC','Temp2:12|12@1+:0.1:-40:-40:180:degC','Temp3:24|12@1+:0.1:-40:-40:180:degC','Temp4:36|12@1+:0.1:-40:-40:180:degC')
  210=@('BrakeLineTemp_Front:0|16@1+:0.1:-40:-40:500:degC');300=@('Angle1:0|16@1+:0.01:0:0:90:deg','Angle2:16|16@1+:0.01:0:0:90:deg')
  700=@('OilTemp_Engine:0|12@1+:0.1:-40:-40:170:degC','OilTemp_Gearbox:12|12@1+:0.1:-40:-40:170:degC')
  800=@('BrakeTemp_FL:0|16@1+:0.1:0:0:150:degC','BrakeTemp_FR:16|16@1+:0.1:0:0:150:degC')
  500=@('Angle1:0|16@1+:0.01:0:0:90:deg','Angle2:16|16@1+:0.01:0:0:90:deg')
  600=@('Temp1:0|12@1+:0.1:-40:-40:180:degC','Temp2:12|12@1+:0.1:-40:-40:180:degC','Temp3:24|12@1+:0.1:-40:-40:180:degC','Temp4:36|12@1+:0.1:-40:-40:180:degC')
  720=@('OilTemp1:0|12@1+:0.1:-40:-40:170:degC','OilTemp2:12|12@1+:0.1:-40:-40:170:degC','BrakeLineTemp_Rear:24|12@1+:0.1:-40:-40:500:degC')
  810=@('BrakeTemp:0|16@1+:0.1:0:0:150:degC')
}
$names=@{200='Front_ThermistorTemps';210='BrakeLineTemp_Front';300='Front_RotPotAngles';500='Rear_RotPotAngles';600='Rear_ThermistorTemps';700='OilTemps';720='Rear_OilTemps';800='BrakeRotorTemps';810='Rear_BrakeRotorTemp'}
foreach($id in $required.Keys){
  if($messages[$id].dlc-ne 8){throw "ID $id DLC"}
  if($messages[$id].name-ne$names[$id]){throw "ID $id message name"}
  if($messages[$id].signals.Count-ne$required[$id].Count){throw "ID $id signal count"}
  foreach($sig in $required[$id]){if($sig -notin $messages[$id].signals){throw "ID $id missing $sig"}}
}
$boundarySignals=0;$representableSignals=0;$unrepresentableSignals=0
foreach($id in $required.Keys){
  foreach($sig in $required[$id]){
    if($sig -notmatch '^([^:]+):(\d+)\|(\d+)@[01][+-]:([^:]+):([^:]+):([^:]+):([^:]+):(.*)$'){throw "ID $id boundary parse $sig"}
    $width=[int]$Matches[3];$factor=[double]$Matches[4];$offset=[double]$Matches[5]
    $minimum=[double]$Matches[6];$maximum=[double]$Matches[7];$rawLimit=([int64]1 -shl $width)-1
    $rawMinimum=[math]::Round(($minimum-$offset)/$factor)
    $rawMaximum=[math]::Round(($maximum-$offset)/$factor)
    ++$boundarySignals
    if($rawMinimum-lt 0-or$rawMaximum-gt$rawLimit){++$unrepresentableSignals;continue}
    ++$representableSignals
    foreach($case in @(
      @{value=$minimum-$factor;inside=$false},@{value=$minimum;inside=$true},
      @{value=$minimum+$factor;inside=$true},@{value=$maximum-$factor;inside=$true},
      @{value=$maximum;inside=$true},@{value=$maximum+$factor;inside=$false})){
      $raw=[math]::Round(($case.value-$offset)/$factor)
      $inside=$case.value-ge$minimum-and$case.value-le$maximum-and$raw-ge 0-and$raw-le$rawLimit
      if($inside-ne$case.inside){throw "ID $id boundary classification failed for $($case.value)"}
      if($inside){
        $decoded=$raw*$factor+$offset
        if([math]::Abs($decoded-$case.value)-gt 1e-9){throw "ID $id boundary round trip failed for $($case.value)"}
      }
    }
  }
}
$rearMax=(4095*0.1)-40
if($rearMax -ne 369.5){throw 'representability calculation error'}
Write-Output "PASS SIL-CAN-006 hash=$hash scope=$($selected -join ',')"
Write-Output "PASS SIL-CAN-004 dbc_boundary_matrix_signals=$boundarySignals representable=$representableSignals unrepresentable=$unrepresentableSignals"
Write-Output "BLOCKED SIL-CAN-004 rear_brake_line_declared_500C_raw_max_12bit=$rearMax"
