param([Parameter(Mandatory=$true)][string]$Root)
$ErrorActionPreference='Stop'
$board=Get-Content -Raw -LiteralPath (Join-Path $Root 'Core\Src\BoardManager.cpp')
$spi=Get-Content -Raw -LiteralPath (Join-Path $Root 'Core\Src\spi.c')
$main=Get-Content -Raw -LiteralPath (Join-Path $Root 'Core\Src\main.c')
$clock=Get-Content -Raw -LiteralPath (Join-Path $Root 'Core\Src\system_stm32g4xx.c')
$requiredBoard=@('spi1\s*\(&hspi1\)','ads1\s*\(spi1,','ads2\s*\(spi1,')
foreach($pattern in $requiredBoard){if($board -notmatch $pattern){throw "ADS SPI ownership mapping missing: $pattern"}}
$requiredSpi=@('hspi1\.Instance\s*=\s*SPI1','SPI_MODE_MASTER','SPI_DIRECTION_2LINES','SPI_DATASIZE_8BIT','SPI_POLARITY_LOW','SPI_PHASE_1EDGE','SPI_NSS_SOFT','SPI_BAUDRATEPRESCALER_16','SPI_FIRSTBIT_MSB')
foreach($pattern in $requiredSpi){if($spi -notmatch $pattern){throw "ADS SPI1 configuration missing: $pattern"}}
foreach($pattern in @('RCC_PLLSOURCE_HSI','PLLM_DIV1','PLLN\s*=\s*10','PLLR_DIV2','APB2CLKDivider\s*=\s*RCC_HCLK_DIV1','MX_SPI1_Init\(\)')){if($main -notmatch $pattern){throw "ADS SPI clock derivation missing: $pattern"}}
if($clock -notmatch '#define\s+HSI_VALUE\s+16000000U'){throw 'Authoritative HSI_VALUE 16 MHz definition missing'}
Write-Output 'PASS VERIFY-ADS-SPI BoardManager_ads1_ads2_to_spi1_hspi1_mode0_8bit_MSB_software_CS_prescaler16_generated_PLL_APB_facts'
# Generated code establishes the PLL/APB clocks and SPI prescaler, but does not
# explicitly select the SPI1 kernel source. Do not infer the RCC_CCIPR reset
# default without a version-locked reference-manual authority.
if($main -notmatch 'PeriphClockSelection\s*=.*RCC_PERIPHCLK_SPI1' -or $main -notmatch 'Spi1ClockSelection\s*='){
  Write-Output 'BLOCKED VERIFY-ADS-SPI-FREQUENCY SPI1_kernel_source_not_explicit_and_version_locked_RCC_CCIPR_default_not_archived'
} else {
  Write-Output 'PASS VERIFY-ADS-SPI-FREQUENCY SPI1_kernel_source_explicit_in_generated_clock_configuration'
}
