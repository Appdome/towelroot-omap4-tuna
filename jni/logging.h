#pragma once

#include <android/log.h>

#ifndef ALOGD
#define ALOGD(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, __VA_ARGS__))
#endif

#ifndef ALOGE
#define ALOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, __VA_ARGS__))
#endif
