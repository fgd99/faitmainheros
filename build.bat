@echo off

set CommonCompilerFlags=-MT -nologo -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4701 -wd4127 -DFAITMAIN_INTERNAL=1 -DFAITMAIN_LENT=1 -DFAITMAIN_WIN32=1 -FC -Z7 -Fmwin32_faitmain.map
set CommonLinkerFlags=-opt:ref user32.lib Gdi32.lib

IF NOT EXIST build mkdir build
pushd build

REM Compilation 32 bits
REM cl %CommonCompilerFlags% ..\code\win32_faitmain.cpp /link -subsystem:windows,5.1 %CommonLinkerFlags%

REM Compilation 64 bits
cl %CommonCompilerFlags% ..\code\win32_faitmain.cpp /link %CommonLinkerFlags%

popd
