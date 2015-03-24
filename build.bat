@echo off

call "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\vcvarsall.bat" x64
set path=C:\Users\Administrateur\Documents\faitmainheros\misc;%path%

IF NOT EXIST build mkdir build
pushd build
cl -MT -nologo -Gm- -GR- -EHa- -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4701 -DFAITMAIN_INTERNAL=1 -DFAITMAIN_LENT=1 -DFAITMAIN_WIN32=1 -FC -Z7 -Fmwin32_faitmain.map ..\code\win32_faitmain.cpp /link -subsystem:windows,5.1 user32.lib Gdi32.lib
popd
