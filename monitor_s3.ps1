# monitor_s3.ps1 - Abre SO o monitor serial da S3 (sem gravar).
# Use p/ ver os logs (ex.: o angulo do AS5047P). Sair: Ctrl+]  Uso: .\monitor_s3.ps1 -Port COM3
param([string]$Port = "COM3")

$TOOLCHAIN_XTENSA = "C:\Users\Vivas\.espressif\tools\xtensa-esp-elf\esp-14.2.0_20241119\xtensa-esp-elf\bin"
$CMAKE      = "C:\Users\Vivas\.espressif\tools\cmake\3.30.2\bin"
$NINJA      = "C:\Users\Vivas\.espressif\tools\ninja\1.12.1"
$PYTHON_BIN = "C:\Users\Vivas\.espressif\python_env\idf5.4_py3.11_env\Scripts"

$env:IDF_PATH            = "C:\Users\Vivas\esp\v5.4\esp-idf"
$env:IDF_PYTHON_ENV_PATH = "C:\Users\Vivas\.espressif\python_env\idf5.4_py3.11_env"
$env:PATH = "$TOOLCHAIN_XTENSA;$CMAKE;$NINJA;$PYTHON_BIN;" + $env:PATH

$python = "$PYTHON_BIN\python.exe"
$idf    = "C:\Users\Vivas\esp\v5.4\esp-idf\tools\idf.py"

Set-Location "D:\USP\Volante\codigos"
Write-Host "[S3 Nucleo] Monitor em $Port (Ctrl+] para sair)..."
& $python $idf -p $Port monitor
