@echo off
rem 命令行编译脚本：DuraEditor (Win64 Development)
rem 用法：tools\Build_DuraEditor.bat [额外UBT参数]
rem 例如增量编译源码：tools\Build_DuraEditor.bat
setlocal
set UE_ROOT=D:\UE\UE_5.8
set PROJECT_ROOT=%~dp0..
"%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" DuraEditor Win64 Development -project="%PROJECT_ROOT%\Dura.uproject" -WaitMutex %*
endlocal & exit /b %ERRORLEVEL%
