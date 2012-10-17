/*
 * Copyright (C) 2012 Texas Instruments
 *
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_POSTPRO_NULL_PATCH
#define ANDROID_POSTPRO_NULL_PATCH

#define POSTPRO_PATCH_JB_PARAMS_SET(a) ((void)0)
#define POSTPRO_PATCH_JB_PARAMS_GET(a, b) ((void)0)
#define POSTPRO_PATCH_JB_OUTPROC_PLAY_INIT(a, b) ((void)0)
#define POSTPRO_PATCH_JB_OUTPROC_PLAY_SAMPLES(a, fmt, buf, bsize, rate, count) ((void)0)
#define POSTPRO_PATCH_JB_OUTPROC_PLAY_EXIT(a, b) ((void)0)
#define POSTPRO_PATCH_JB_OUTPROC_PLAY_ROUTE(a, para, val) ((void)0)
#define POSTPRO_PATCH_JB_OUTPROC_DUPE_SAMPLES(a, fmt, buf, bsize, rate, count) ((void)0)
#define POSTPRO_PATCH_JB_INPROC_INIT(a, b, fmt) ((void)0)
#define POSTPRO_PATCH_JB_INPROC_SAMPLES(a, fmt, buf, bsize, rate, count) ((void)0)
#define POSTPRO_PATCH_JB_INPROC_EXIT(a, b, fmt) ((void)0)
#define POSTPRO_PATCH_JB_INPROC_ROUTE(a, para, val) ((void)0)

#endif // ANDROID_POSTPRO_NULL_PATCH
