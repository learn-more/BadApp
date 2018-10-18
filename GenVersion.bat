@echo off

setlocal

for /f %%i in ('git describe --always --dirty') do set GIT_VERSION=%%i
set FULL_STRING=#define GIT_VERSION_STR "%GIT_VERSION%"
if exist generated_git_version.h (
	set /p CURRENT_STRING=<generated_git_version.h
) else (
	set CURRENT_STRING=_does_not_exist_
)
if "!%FULL_STRING%!" == "!%CURRENT_STRING%!" goto DONE

echo %FULL_STRING%> generated_git_version.h
echo Version:%GIT_VERSION%

:DONE

endlocal
