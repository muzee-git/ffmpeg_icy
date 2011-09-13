PREFIX=android-mips
NDK=/home/qrtt1/app/android-ndk-r6m-linux
MY_CC=$NDK/toolchains/mips-linux-android-4.4.3/prebuilt/linux-x86/bin/mips-linux-android-gcc
MY_NM=$NDK/toolchains/mips-linux-android-4.4.3/prebuilt/linux-x86/bin/mips-linux-android-nm
          
MY_CC_PREFIX=$NDK/toolchains/mips-linux-android-4.4.3/prebuilt/linux-x86/bin/mips-linux-android-
PLATFORM=$NDK/platforms/android-4/arch-mips
MY_LIB_GCC_A=$NDK/toolchains/mips-linux-android-4.4.3/prebuilt/linux-x86/lib/gcc/mips-linux-android/4.4.3/libgcc.a

OPTIMIZE_CFLAGS="-EL -march=mips32 -mips32 -mhard-float"
#    --disable-shared \


make install

"$MY_CC_PREFIX"ar d libavcodec/libavcodec.a inverse.o
"$MY_CC_PREFIX"ld -rpath-link=$PLATFORM/usr/lib -L$PLATFORM/usr/lib -soname libmuffmpeg.so \
    -shared -nostdlib -z,noexecstack -Bsymbolic --whole-archive --no-undefined -o $PREFIX/libmuffmpeg.so \
    libavcodec/libavcodec.a libavformat/libavformat.a libavutil/libavutil.a libswscale/libswscale.a \
    -EL -lc -lm -lz -ldl -llog --warn-once --dynamic-linker=/system/bin/linker \
    $MY_LIB_GCC_A
