# flash_botoeira_nomon.ps1 - Grava o firmware na Botoeira C3 SEM abrir monitor.
# Uso: .\flash_botoeira_nomon.ps1 -Port COMx
param([string]$Port = "COM4")

$TOOLCHAIN_RISCV = "C:\Users\Vivas\.espressif\tools\riscv32-esp-elf\esp-14.2.0_20241119\riscv32-esp-elf\bin"
$CMAKE      = "C:\Users\Vivas\.espressif\tools\cmake\3.30.2\bin"
$NINJA      = "C:\Users\Vivas\.espressif\tools\ninja\1.12.1"
$PYTHON_BIN = "C:\Users\Vivas\.espressif\python_env\idf5.4_py3.11_env\Scripts"

$env:IDF_PATH            = "C:\Users\Vivas\esp\v5.4\esp-idf"
$env:IDF_PYTHON_ENV_PATH = "C:\Users\Vivas\.espressif\python_env\idf5.4_py3.11_env"
$env:PATH = "$TOOLCHAIN_RISCV;$CMAKE;$NINJA;$PYTHON_BIN;" + $env:PATH

$python = "$PYTHON_BIN\python.exe"
$idf    = "C:\Users\Vivas\esp\v5.4\esp-idf\tools\idf.py"

Set-Location "D:\USP\Volante\codigos_c3_botoeira"
Write-Host "[Botoeira] Gravando em $Port (sem monitor)..."
& $python $idf -p $Port flash
$code = $LASTEXITCODE
if ($code -eq 0) { Write-Host "[Botoeira] FLASH OK" } else { Write-Host "[Botoeira] FLASH FALHOU (exit $code)" }
exit $code
