@echo off
rem Build ALIF tools with gcc. No make required.
rem Run from the repo root:  build.bat

setlocal
set CFLAGS=-std=c11 -Wall -Wextra -Werror -I include

if /I "%1"=="afas" goto afas
if /I "%1"=="alif" goto alif
if /I "%1"=="alifc" goto alifc
if /I "%1"=="smoke" goto smoke

:alif
echo gcc -o alif.exe
gcc %CFLAGS% -o alif.exe src\alif.c src\load.c src\vm.c
if errorlevel 1 exit /b 1
if /I "%1"=="alif" goto ship

:afas
echo gcc -o afas\afas.exe
gcc %CFLAGS% -o afas\afas.exe afas\afas.c src\load.c
if errorlevel 1 exit /b 1
if /I "%1"=="afas" goto ship

:alifc
echo gcc -o alifc\alifc.exe
gcc -std=c11 -Wall -Wextra -Werror -o alifc\alifc.exe alifc\alifc.c
if errorlevel 1 exit /b 1
goto ship

:smoke
echo gcc -o tests\smoke.exe
gcc %CFLAGS% -o tests\smoke.exe tests\smoke.c src\vm.c
if errorlevel 1 exit /b 1
goto done

:ship
if not exist ship mkdir ship
if exist alif.exe (
    copy /Y alif.exe ship\alif.exe >nul
    if errorlevel 1 exit /b 1
)
if exist afas\afas.exe (
    copy /Y afas\afas.exe ship\afas.exe >nul
    if errorlevel 1 exit /b 1
)
if exist alifc\alifc.exe (
    copy /Y alifc\alifc.exe ship\alifc.exe >nul
    if errorlevel 1 exit /b 1
)
copy /Y scripts\run.bat ship\run.bat >nul
if errorlevel 1 exit /b 1
copy /Y scripts\SHIP.txt ship\README.txt >nul
if errorlevel 1 exit /b 1
echo ship\

:done
echo ok
endlocal
