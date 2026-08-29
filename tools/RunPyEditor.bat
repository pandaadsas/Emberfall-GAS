@echo off
rem Run FULL UE editor with python script. Usage: RunPyEditor.bat script.py
setlocal
set UE_ROOT=D:\UE\UE_5.8
set PROJECT_ROOT=%~dp0..
"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe" "%PROJECT_ROOT%\Dura.uproject" -unattended -nosplash -nullrhi -ExecutePythonScript="%PROJECT_ROOT%\tools\%1"
endlocal & exit /b %ERRORLEVEL%
