# build_botoeira.ps1 - Compila SO o firmware da Botoeira C3 (build only).
# 1a vez: rode antes  idf set-target esp32c3  (este script faz isso se faltar sdkconfig).
# Uso: .\build_botoeira.ps1

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
if (-not (Test-Path "sdkconfig")) {
    Write-Host "[Botoeira] set-target esp32c3 (1a vez)..."
    & $python $idf set-target esp32c3
}
Write-Host "[Botoeira] Compilando (build only)..."
& $python $idf build
$code = $LASTEXITCODE
if ($code -eq 0) { Write-Host "[Botoeira] BUILD OK" } else { Write-Host "[Botoeira] BUILD FALHOU (exit $code)" }
exit $code
