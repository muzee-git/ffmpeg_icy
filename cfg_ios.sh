export PATH=.:$PATH
MY_CC=/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/gcc
#MY_SYSROOT=/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS4.3.sdk
MY_SYSROOT=/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS5.0.sdk
MY_CPU=arm7
MY_ARCH=armv7

./configure --enable-cross-compile --target-os=darwin --arch=c \
--cc=$MY_CC \
--as="gas-preprocessor.pl --disable-neon --disable-pic --enable-postproc --disable-debug --disable-stripping $MY_CC" \
--sysroot=$MY_SYSROOT \
--cpu=$MY_CPU --extra-cflags="-arch $MY_ARCH -isysroot $MY_SYSROOT" --extra-ldflags="-arch $MY_ARCH -isysroot $MY_SYSROOT" \
--prefix=$(pwd)/O_$MY_CPU \
--disable-doc --disable-bzlib --disable-ffmpeg --disable-ffplay \
--disable-ffserver --disable-ffprobe --disable-avdevice --disable-avfilter --disable-filters \
--disable-bsfs --disable-protocol=udp --disable-muxers --disable-lpc
make -j8 install
