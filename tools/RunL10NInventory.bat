@echo off
rem 无头编辑器运行资产汉化盘点脚本（只读）
setlocal
set UE_ROOT=D:\UE\UE_5.8
set PROJECT_ROOT=%~dp0..
"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJECT_ROOT%\Dura.uproject" -run=pythonscript -script="%PROJECT_ROOT%\tools\l10n_inventory.py" -stdout -unattended -nopause -nosplash
endlocal & exit /b %ERRORLEVEL%
