param([Parameter(Mandatory=$true)][string]$MainPath)
$ErrorActionPreference='Stop'
$text=Get-Content -Raw -LiteralPath $MainPath
$health=$text.IndexOf('ok = BoardManager_checkADCPower(bm)')
$update=$text.IndexOf('if (BoardManager_publishAllChannels(bm))')
$publish=$text.IndexOf('BoardManager_fdcan_bulk_tx(bm)',[Math]::Max($update,0))
$delay=$text.IndexOf('HAL_Delay(4000)',[Math]::Max($publish,0))
if($health-lt 0-or$update-lt 0-or$publish-lt 0-or$delay-lt 0-or-not($health-lt$update-and$update-lt$publish-and$publish-lt$delay)){
  throw 'SIL-BM-007 main-loop source sequence no longer matches the runtime characterization'
}
if($text -match 'if\s*\(\s*ok\s*\)'){
  throw 'XPASS SIL-BM-007 health result is now consumed; retire DEF-BM-HEALTH-IGNORED-BEFORE-PUBLISH and its CHAR case'
}
Write-Output 'PASS SIL-BM-007 source sequence health-update-publish-delay'
Write-Output 'PASS SIL-BM-007 static_characterization_DEF-BM-HEALTH-IGNORED-BEFORE-PUBLISH'
