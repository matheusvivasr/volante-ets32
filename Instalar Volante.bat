@echo off
REM ===========================================================
REM  Volante DIY - atalho de instalacao (clique duas vezes)
REM  Abre o instalador com menu (installer.py).
REM ===========================================================
title Volante DIY - Instalador
cd /d "%~dp0"

REM Tenta o lancador 'py' (padrao no Windows); se nao, tenta 'python'.
where py >nul 2>nul
if %errorlevel%==0 (
    py installer.py
    goto fim
)
where python >nul 2>nul
if %errorlevel%==0 (
    python installer.py
    goto fim
)

echo.
echo [ERRO] Python nao encontrado.
echo Instale o Python 3 em https://www.python.org/downloads/
echo e marque a opcao "Add Python to PATH" durante a instalacao.
echo.

:fim
echo.
pause
