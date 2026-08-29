@echo off
rem Run UE headless python script. Usage: RunPy.bat script.py
setlocal
set UE_ROOT=D:\UE\UE_5.8
set PROJECT_ROOT=%~dp0..
"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJECT_ROOT%\Dura.uproject" -run=pythonscript -script="%PROJECT_ROOT%\tools\%1" -stdout -unattended -nopause -nosplash
endlocal & exit /b %ERRORLEVEL%
