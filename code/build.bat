@echo off

set CommonCompilerFlags= -MTd -nologo -Gm-  /Wv:18  -GR- -EHsc -Oi -W4 -WX -wd4201 -wd4100 -wd4189 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 -FC -Z7
set CommonLinkerFlags= -incremental:no -opt:ref  user32.lib gdi32.lib winmm.lib


REM TODO  can we do a build in 32 bit and 64 on one exe? opt:ref this removes stuff from .map that are not needed 
REM /link -subsystem:windows,5.1 this makes it compile exe to other windows
REM -MT means use static library and there is no dll error

IF NOT EXIST ..\..\build mkdir ..\..\build
pushd ..\..\build

REM 32-bit build
REM cl %CommonCompilerFlags% ..\handmade\code\win32_handmade.cpp /link -subsystem:windows,5.1  %CommonLinkerFlags%

REM 64-bit
del *.pdb > NUL 2> NUL
cl %CommonCompilerFlags% ..\handmade\code\handmade.cpp -Fmhandmade.map -LD -link -incremental:no /PDB:handmade_%random%.pdb /EXPORT:GameGetSoundSamples /EXPORT:GameUpdateAndRender
cl %CommonCompilerFlags% ..\handmade\code\win32_handmade.cpp -Fmwin32_handmade.map /link %CommonLinkerFlags%
popd