@echo off

set Config=Debug32
set CXXFlags=-fno-builtin -g -O0
if "%1"=="Release" (
	set Config=Release32
	set CXXFlags=-DNDEBUG -O3 -ffunction-sections -fdata-sections -Wl,--gc-sections,-s
)

call mingw 32

mcfbuild -p../../MCFCRT/MCFBuild.mcfproj -s../../MCFCRT -d../../.Built/%Config%/mcfcrt -o../../.Built/%Config%/libmcfcrt.a %* || exit /b 1
mcfbuild -p../../MCF/MCFBuild.mcfproj -s../../MCF -d../../.Built/%Config%/mcf -o../../.Built/%Config%/libmcf.a %* || exit /b 1
mcfbuild -p../../External/lzma/MCFBuild.mcfproj -s../../External/lzma -d../../.Built/%Config%/lzma -o../../.Built/%Config%/liblzma.a %* || exit /b 1
mcfbuild -p../../External/zlib/MCFBuild.mcfproj -s../../External/zlib -d../../.Built/%Config%/zlib -o../../.Built/%Config%/libz.a %* || exit /b 1

g++ %CXXFlags% -std=c++1y -Wall -Wextra -Wsuggest-attribute=noreturn -pipe -mfpmath=sse,387 -msse2 -masm=intel main.cpp -o ".%Config%.exe" -I../.. -L../../.Built/%Config% -static -nostartfiles -Wl,-e__MCFExeStartup,--disable-runtime-pseudo-reloc,--disable-auto-import,-lmcf,-lmcfcrt,-lstdc++,-lgcc,-lgcc_eh,-lmingwex,-lmcfcrt,-lws2_32,-lwinhttp,-lz || exit /b 1
