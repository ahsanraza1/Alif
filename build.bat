@echo off
rem Build ALIF tools with gcc. No make required.
rem Run from the repo root:  build.bat

setlocal
set CFLAGS=-std=c11 -Wall -Wextra -Werror -I include

if /I "%1"=="afas" goto afas
if /I "%1"=="alif" goto alif
if /I "%1"=="smoke" goto smoke

:alif
echo gcc -o alif.exe
gcc %CFLAGS% -o alif.exe src\alif.c src\load.c src\vm.c
if errorlevel 1 exit /b 1
if /I "%1"=="alif" goto done

:afas
echo gcc -o afas\afas.exe
gcc %CFLAGS% -o afas\afas.exe afas\afas.c src\load.c
if errorlevel 1 exit /b 1
goto done

:smoke
echo gcc -o tests\smoke.exe
gcc %CFLAGS% -o tests\smoke.exe tests\smoke.c src\vm.c
if errorlevel 1 exit /b 1
goto done

:done
echo ok
endlocal
