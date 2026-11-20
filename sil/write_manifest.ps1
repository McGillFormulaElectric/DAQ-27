param(
  [Parameter(Mandatory=$true)][string]$Root,
  [Parameter(Mandatory=$true)][string]$DbcPath,
  [Parameter(Mandatory=$true)][string]$OutPath
)
$ErrorActionPreference='Stop'
$evidence=Join-Path $PSScriptRoot 'evidence'
$inputs=@(
  (Join-Path $Root 'ads7066-reference.pdf'),
  (Join-Path $evidence 'ADS7066-RevC-SBAS928C.pdf'),
  (Join-Path $evidence 'SBAA456-ADS7066-CRC.pdf'),
    (Join-Path $evidence 'ads7066_evidence.csv'),
    (Join-Path $evidence 'ads7066_figure_pin_traces.csv'),
  (Join-Path $Root 'sil\ads7066_requirements.csv'),
  (Join-Path $Root 'sil\sil_instructions.md'),
  (Join-Path $Root 'sil\ads7066_model.hpp'),
  (Join-Path $Root 'sil\ads7066_model.cpp'),
  (Join-Path $Root 'sil\ads7066_bus.hpp'),
  (Join-Path $Root 'sil\ads7066_bus.cpp'),
  (Join-Path $Root 'sil\ads7066_fault_overlay.hpp'),
  (Join-Path $Root 'sil\ads7066_fault_overlay.cpp'),
  (Join-Path $Root 'sil\test_ads7066_model.cpp'),
  (Join-Path $Root 'sil\fakes\fake_hal.hpp'),
  (Join-Path $Root 'sil\fakes\fake_hal.cpp'),
  (Join-Path $Root 'sil\fakes\virtual_can.hpp'),
  (Join-Path $Root 'sil\fakes\virtual_can.cpp'),
  (Join-Path $Root 'sil\fakes\fake_fdcan.hpp'),
  (Join-Path $Root 'sil\fakes\fake_fdcan.cpp'),
  (Join-Path $Root 'sil\test_can.cpp'),
  (Join-Path $Root 'sil\test_adc.cpp'),
  (Join-Path $Root 'sil\test_application.cpp'),
  (Join-Path $Root 'sil\validate_ads7066_requirements.ps1'),
  (Join-Path $Root 'sil\verify_adc_spi.ps1'),
  (Join-Path $Root 'sil\run_sil.ps1'),
  (Join-Path $Root 'sil\CMakeLists.txt'),
  (Join-Path $Root 'sil\collect_coverage.ps1'),
  (Join-Path $Root 'sil\write_manifest.ps1'),
  (Join-Path $Root 'sil\archive_run.ps1'),
  (Join-Path $Root 'schematic-reference.pdf'),
  $DbcPath,
  (Join-Path $evidence 'MFE26_Sensor_DBC_Mapping.csv'),
  (Join-Path $evidence 'KTY84.pdf'),
  (Join-Path $evidence 'BRAKE_ROTOR_TEMP_INFKL-800.pdf'),
  (Join-Path $evidence 'ROT_POT_RE22.pdf'),
  (Join-Path $evidence 'AAS-920-672B-Thermometrics-GE1711.pdf'),
  (Join-Path $Root 'sil\traceability.csv'),
  (Join-Path $Root 'sil\defect_details.csv'),
  (Join-Path $Root 'Drivers\MFE26-G4-Drivers\Src\Device Drivers\ADS7066.cpp'),
  (Join-Path $Root 'Core\Src\BoardManager.cpp'),
  (Join-Path $Root 'Core\Src\Wrapper.cpp'),
  (Join-Path $Root 'Core\Src\main.c'),
  (Join-Path $Root 'Core\Src\spi.c'),
  (Join-Path $Root 'Drivers\MFE26-G4-Drivers\Src\HAL Drivers\CANDriver.cpp'),
  (Join-Path $Root 'Core\Src\fdcan.c'),
  (Join-Path $Root 'Drivers\STM32G4xx_HAL_Driver\Src\stm32g4xx_hal_fdcan.c')
)
$rootFull=[IO.Path]::GetFullPath($Root).TrimEnd([IO.Path]::DirectorySeparatorChar)+[IO.Path]::DirectorySeparatorChar
$files=@($inputs|ForEach-Object{
  $resolved=[IO.Path]::GetFullPath($_)
  $relative=if($resolved.StartsWith($rootFull,[StringComparison]::OrdinalIgnoreCase)){$resolved.Substring($rootFull.Length)}else{"external\$([IO.Path]::GetFileName($resolved))"}
  if(Test-Path -LiteralPath $resolved){$h=Get-FileHash -Algorithm SHA256 -LiteralPath $resolved;[ordered]@{path=$relative;sha256=$h.Hash;bytes=(Get-Item -LiteralPath $resolved).Length}}
  else{[ordered]@{path=$relative;missing=$true}}
})
$tools=[ordered]@{}
foreach($name in @('g++','gcov','cmake','clang++','clang-tidy','cppcheck','arm-none-eabi-gcc')){$cmd=Get-Command $name -ErrorAction SilentlyContinue;$tools[$name]=if($cmd){$cmd.Source}else{$null}}
$commit=(& git -C $Root rev-parse HEAD).Trim()
$manifest=[ordered]@{generated_utc=(Get-Date).ToUniversalTime().ToString('o');commit=$commit;files=$files;tools=$tools;compiler=((& g++ --version|Select-Object -First 1).ToString())}
$manifest|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 -LiteralPath $OutPath
Write-Output "PASS MANIFEST $OutPath"
