@echo off
rem Chain the three shipped tools. Does not merge them.
rem   run.bat <program.alif>
setlocal EnableExtensions

set "HERE=%~dp0"
if "%HERE:~-1%"=="\" set "HERE=%HERE:~0,-1%"

if "%~1"=="" (
    echo usage: run ^<program.alif^>
    echo.
    echo Compiles with alifc.exe, assembles with afas.exe, runs with alif.exe.
    echo The tools stay separate; this script only calls them in order.
    exit /b 1
)

if /I not "%~x1"==".alif" (
    echo run: source must be a .alif file
    exit /b 1
)

if not exist "%~1" (
    echo run: cannot find %~1
    exit /b 1
)

if not exist "%HERE%\alifc.exe" (
    echo run: missing alifc.exe in %HERE%
    exit /b 1
)
if not exist "%HERE%\afas.exe" (
    echo run: missing afas.exe in %HERE%
    exit /b 1
)
if not exist "%HERE%\alif.exe" (
    echo run: missing alif.exe in %HERE%
    exit /b 1
)

"%HERE%\alifc.exe" "%~1"
if errorlevel 1 exit /b 1

"%HERE%\afas.exe" "%~dpn1.afb"
if errorlevel 1 exit /b 1

"%HERE%\alif.exe" "%~dpn1.afbin"
exit /b %ERRORLEVEL%
