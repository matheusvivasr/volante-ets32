# build_c3.ps1 - Compila SO o firmware do Painel C3 (build only).
# NAO grava e NAO abre monitor: compila e SAI. Use flash_c3_painel.ps1 para gravar.
# Uso: .\build_c3.ps1

$TOOLCHAIN_RISCV = "C:\Users\Vivas\.espressif\tools\riscv32-esp-elf\esp-14.2.0_20241119\riscv32-esp-elf\bin"
$CMAKE      = "C:\Users\Vivas\.espressif\tools\cmake\3.30.2\bin"
$NINJA      = "C:\Users\Vivas\.espressif\tools\ninja\1.12.1"
$PYTHON_BIN = "C:\Users\Vivas\.espressif\python_env\idf5.4_py3.11_env\Scripts"

$env:IDF_PATH            = "C:\Users\Vivas\esp\v5.4\esp-idf"
$env:IDF_PYTHON_ENV_PATH = "C:\Users\Vivas\.espressif\python_env\idf5.4_py3.11_env"
$env:PATH = "$TOOLCHAIN_RISCV;$CMAKE;$NINJA;$PYTHON_BIN;" + $env:PATH

$python = "$PYTHON_BIN\python.exe"
$idf    = "C:\Users\Vivas\esp\v5.4\esp-idf\tools\idf.py"

Set-Location "D:\USP\Volante\codigos_c3_painel"
Write-Host "[C3 Painel] Compilando (build only)..."
& $python $idf build
$code = $LASTEXITCODE
if ($code -eq 0) { Write-Host "[C3 Painel] BUILD OK" } else { Write-Host "[C3 Painel] BUILD FALHOU (exit $code)" }
exit $code
