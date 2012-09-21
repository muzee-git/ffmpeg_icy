PKG_CONFIG_PATH="/opt/rtmpdump/lib/pkgconfig" ./configure --disable-doc \
--disable-ffserver --disable-avdevice \
--disable-postproc --disable-avfilter --disable-bsfs \
--disable-filters \
--disable-asm \
--disable-bzlib \
--enable-librtmp \
--prefix=/opt/ffmpeg
