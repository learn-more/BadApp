@echo off

setlocal

for /f %%i in ('git describe --always --dirty') do set GIT_VERSION=%%i
set FULL_STRING=#define GIT_VERSION_DESC  "%GIT_VERSION%"
if exist generated_git_version.h (
    set /p CURRENT_STRING=<generated_git_version.h
) else (
    set CURRENT_STRING=_does_not_exist_
)
if "!%FULL_STRING%!" == "!%CURRENT_STRING%!" goto DONE


for /F "tokens=1,2,3,4,5 delims=.-" %%a in ("%GIT_VERSION%") do (
    set GIT_MAJOR=%%a
    set GIT_MINOR=%%b
    set GIT_REV=%%c
    set GIT_HASH=%%d
    if [%%e] == [] (
        set GIT_DIRTY=0
    ) else (
        set GIT_DIRTY=77
    )
)

(echo %FULL_STRING%)> generated_git_version.h
(echo #define GIT_VERSION_MAJOR %GIT_MAJOR%)>> generated_git_version.h
(echo #define GIT_VERSION_MINOR %GIT_MINOR%)>> generated_git_version.h
(echo #define GIT_VERSION_REV   %GIT_REV%)>> generated_git_version.h
(echo #define GIT_VERSION_DIRTY %GIT_DIRTY%)>> generated_git_version.h
(echo #define GIT_VERSION_HASH  "%GIT_HASH%")>> generated_git_version.h

echo Version: %GIT_MAJOR%.%GIT_MINOR%.%GIT_REV%.%GIT_DIRTY%-%GIT_HASH%

:DONE

endlocal
