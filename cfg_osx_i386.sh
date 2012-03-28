#export ARCHFLAGS="-arch i386"
#export CFLAGS="-arch i386"
./configure --prefix=$(pwd)/O_i386 --disable-yasm --arch=i386 --cc='gcc -arch i386' \
--disable-doc --disable-bzlib --disable-ffmpeg --disable-ffplay --disable-muxers \
--disable-avdevice --disable-avfilter
#--disable-encoders
