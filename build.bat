@echo off

call "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\vcvarsall.bat" x64
set path=C:\Users\Administrateur\Documents\faitmainheros\misc;%path%

IF NOT EXIST build mkdir build
pushd build
cl -GR- -EHa- -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -DFAITMAIN_INTERNAL=1 -DFAITMAIN_LENT=1 -DFAITMAIN_WIN32=1 -FC -Zi ..\code\win32_faitmain.cpp user32.lib Gdi32.lib
popd
