#ifndef AVFORMAT_MMS_PLUS_H
#define AVFORMAT_MMS_PLUS_H

int mms_plus_video_index(void);
int mms_plus_audio_index(void);

void mms_plus_set_video_index_cb(int (*cb)(void));
void mms_plus_set_audio_index_cb(int (*cb)(void));

#endif

