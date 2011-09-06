#include "mms_plus.h"
#include <stdio.h>


static int mms_plus_default_video_index_cb(void)
{
    return -1;
}

static int mms_plus_default_audio_index_cb(void)
{
    return -1;
}

static int (*mms_plus_video_index_cb)(void) = mms_plus_default_video_index_cb;
static int (*mms_plus_audio_index_cb)(void) = mms_plus_default_audio_index_cb;

int mms_plus_video_index(void)
{
    return mms_plus_video_index_cb();
}

int mms_plus_audio_index(void)
{
    return mms_plus_audio_index_cb();
}

void mms_plus_set_video_index_cb(int (*cb)(void))
{
    if(cb != NULL)
        mms_plus_video_index_cb = cb;
    else
        mms_plus_video_index_cb = mms_plus_default_video_index_cb;
}

void mms_plus_set_audio_index_cb(int (*cb)(void))
{
    if(cb != NULL)
        mms_plus_audio_index_cb = cb;
    else
        mms_plus_audio_index_cb = mms_plus_default_audio_index_cb;
}
