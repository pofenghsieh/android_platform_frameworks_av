/*
 * Copyright (C) Texas Instruments - http://www.ti.com/
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

#define LOG_TAG "wfd"
#include <utils/Log.h>

#include "../ElementaryParser.h"
#include "../VideoParameters.h"
#include "../AudioParameters.h"
#include "../UibcParameters.h"
#include "OMX_Video.h"

namespace android {

const char kElementary[] = "01, 03 458001A0; AAC/Ac3";
const char * kElementaryTable[] = {"LPCM", "AAC", "AC3", NULL};

const char kSink1[] = "00 00 01 01 00000001 00000000 00000000 00 0000 0000 00 none none";
const char kSink2[] = "00 00 01 01 0001ffff 3fffffff 00000fff 00 0000 0000 00 none none"
                            ", 02 02 00000001 00000033 00000044 00 0000 0000 00 none none";

const char kSource1[] = "00 00 01 01 00001111 00001111 00000111 00 0000 0000 00 none none";
const char kSource2[] = "00 00 02 02 00001111 00001111 00000111 00 0000 0000 00 none none";
const char kSource3[] = "00 00 01 01 00012345 12345678 00000123 00 0000 0000 00 none none";
const char kSource4[] = "00 00 01 01 00000004 00000000 00000000 00 0000 0000 00 none none";

const char kSinkErrProfile1[] = "00 00 00 01 00000001 00000000 00000000 00 0000 0000 00 none none";
const char kSinkErrProfile2[] = "00 00 03 01 00000001 00000000 00000000 00 0000 0000 00 none none";
const char kSinkErrProfile3[] = "00 00 04 01 00000001 00000000 00000000 00 0000 0000 00 none none";
const char kSinkErrProfile4[] = "00 00 05 01 00000001 00000000 00000000 00 0000 0000 00 none none";
const char kSinkErrProfile5[] = "00 00 1 01 00000001 00000000 00000000 00 0000 0000 00 none none";
const char kSinkErrLevel1[] = "00 00 01 00 00000001 00000000 00000000 00 0000 0000 00 none none";
const char kSinkErrLevel2[] = "00 00 01 03 00000001 00000000 00000000 00 0000 0000 00 none none";
const char kSinkErrLevel3[] = "00 00 01 20 00000001 00000000 00000000 00 0000 0000 00 none none";
const char kSinkErrLevel4[] = "00 00 01 21 00000001 00000000 00000000 00 0000 0000 00 none none";
const char kSinkErrLevel5[] = "00 00 01 1 00000001 00000000 00000000 00 0000 0000 00 none none";
const char kSinkErrCea1[] = "00 00 01 01 00020000 00000000 00000000 00 0000 0000 00 none none";
const char kSinkErrCea2[] = "00 00 01 01 00020001 00000000 00000000 00 0000 0000 00 none none";
const char kSinkErrCea3[] = "00 00 01 01 0000001 00000000 00000000 00 0000 0000 00 none none";
const char kSinkErrVesa1[] = "00 00 01 01 00000000 40000000 00000000 00 0000 0000 00 none none";
const char kSinkErrVesa2[] = "00 00 01 01 00000000 40000001 00000000 00 0000 0000 00 none none";
const char kSinkErrVesa3[] = "00 00 01 01 00000000 0000001 00000000 00 0000 0000 00 none none";
const char kSinkErrHh1[] = "00 00 01 01 00000000 00000000 00001000 00 0000 0000 00 none none";
const char kSinkErrHh2[] = "00 00 01 01 00000000 00000000 00001001 00 0000 0000 00 none none";
const char kSinkErrHh3[] = "00 00 01 01 00000000 00000000 0000001 00 0000 0000 00 none none";
const char kSinkErrCVH1[] = "00 00 01 01 00000000 00000000 00000000 00 0000 0000 00 none none";
const char kSinkErrLatency1[] = "00 00 01 01 00000001 00000000 00000000 0 0000 0000 00 none none";
const char kSinkErrMinSliceSize1[] = "00 00 01 01 00000001 00000000 00000000 00 000 0000 00 none none";
const char kSinkErrSliceEnc1[] = "00 00 01 01 00000001 00000000 00000000 00 0000 2000 00 none none";
const char kSinkErrSliceEnc2[] = "00 00 01 01 00000001 00000000 00000000 00 0000 000 00 none none";
const char kSinkErrFrameRate1[] = "00 00 01 01 00000001 00000000 00000000 00 0000 0000 20 none none";
const char kSinkErrFrameRate2[] = "00 00 01 01 00000001 00000000 00000000 00 0000 0000 0 none none";
const char kSinkErrMaxHres1[] = "00 00 01 01 00000001 00000000 00000000 00 0000 0000 00 011h none";
const char kSinkErrMaxHres2[] = "00 00 01 01 00000001 00000000 00000000 00 0000 0000 00 nane none";
const char kSinkErrMaxVres1[] = "00 00 01 01 00000001 00000000 00000000 00 0000 0000 00 none 123h";
const char kSinkErrMaxVres2[] = "00 00 01 01 00000001 00000000 00000000 00 0000 0000 00 none nona";

const char kB31x1680x1024x60[] = "00 00 01 01 00000000 02000000 00000000 00 0000 0000 00 none none";

const char * kSinkErr[] = {
    kSinkErrProfile1,
    kSinkErrProfile2,
    kSinkErrProfile3,
    kSinkErrProfile4,
    kSinkErrProfile5,
    kSinkErrLevel1,
    kSinkErrLevel2,
    kSinkErrLevel3,
    kSinkErrLevel4,
    kSinkErrLevel5,
    kSinkErrCea1,
    kSinkErrCea2,
    kSinkErrCea3,
    kSinkErrVesa1,
    kSinkErrVesa2,
    kSinkErrVesa3,
    kSinkErrHh1,
    kSinkErrHh2,
    kSinkErrHh3,
    kSinkErrCVH1,
    kSinkErrLatency1,
    kSinkErrMinSliceSize1,
    kSinkErrSliceEnc1,
    kSinkErrSliceEnc2,
    kSinkErrFrameRate1,
    kSinkErrFrameRate2,
    kSinkErrMaxHres1,
    kSinkErrMaxHres2,
    kSinkErrMaxVres1,
    kSinkErrMaxVres2,
};

const char * kApplyVideoMode1[] = {
    kSink1,
    kSource4,
};

const char kSinkAudio1[] = "LPCM 00000002 00";
const char kSinkAudio2[] = "LPCM 00000002 00, AAC 00000003 00, AC3 00000007 00";

const char kSourceAudio1[] = "AAC 00000002 00";
const char kSourceAudio2[] = "LPCM 00000003 00, AAC 0000000f 00, AC3 00000007 00";

const char kSinkAudioErrFormat1[] = "LPCN 00000002 00";
const char kSinkAudioErrModes1[] = "LPCM 00000000 00";
const char kSinkAudioErrModes2[] = "LPCM 00000004 00";
const char kSinkAudioErrModes3[] = "LPCM 00000005 00";
const char kSinkAudioErrModes4[] = "LPCM 0000002 00";
const char kSinkAudioErrModes5[] = "AAC 00000010 00";
const char kSinkAudioErrModes6[] = "AC3 00000008 00";
const char kSinkAudioErrLatency1[] = "LPCM 00000002 0";

const char kAac48000x16x4[] = "AAC 00000002 00";

const char * kSinkAudioErr[] = {
    kSinkAudioErrFormat1,
    kSinkAudioErrModes1,
    kSinkAudioErrModes2,
    kSinkAudioErrModes3,
    kSinkAudioErrModes4,
    kSinkAudioErrModes5,
    kSinkAudioErrModes6,
    kSinkAudioErrLatency1,
};

const char * kApplyAudioMode1[] = {
    kSinkAudio1,
    kSourceAudio1,
};

const char kSinkUibc1[] = "none";
const char kSinkUibc2[] = "input_category_list=none; generic_cap_list=none; hidc_cap_list=none; port=none";
const char kSinkUibc3[] = "input_category_list=GENERIC; generic_cap_list=SingleTouch; hidc_cap_list=none; port=none";
const char kSinkUibc4[] = "input_category_list=GENERIC; generic_cap_list=Mouse, SingleTouch; hidc_cap_list=none; port=none";
const char kSinkUibc5[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=RemoteControl/Infrared; port=none";
const char kSinkUibc6[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=Mouse/BT, RemoteControl/Infrared; port=none";
const char kSinkUibc7[] = "input_category_list=GENERIC, HIDC; generic_cap_list=Mouse, SingleTouch; hidc_cap_list=Mouse/BT, RemoteControl/Infrared; port=none";

const char * kSinkUibc[] = {
    kSinkUibc1,
    kSinkUibc2,
    kSinkUibc3,
    kSinkUibc4,
    kSinkUibc5,
    kSinkUibc6,
    kSinkUibc7,
};

const char kSinkUibcErr1[] = "none;";
const char kSinkUibcErr2[] = "nane";
const char kSinkUibcErr3[] = "input_categori_list=none; generic_cap_list=none; hidc_cap_list=none; port=none";
const char kSinkUibcErr4[] = "input_category_list= none; generic_cap_list=none; hidc_cap_list=none; port=none";
const char kSinkUibcErr5[] = "input_category_list=nome; generic_cap_list=none; hidc_cap_list=none; port=none";
const char kSinkUibcErr6[] = "input_category_list=none, generic_cap_list=none; hidc_cap_list=none; port=none";
const char kSinkUibcErr7[] = "input_category_list=none; generic_cop_list=Mouse; hidc_cap_list=none; port=none";
const char kSinkUibcErr8[] = "input_category_list=none; generic_cap_list= none; hidc_cap_list=none; port=none";
const char kSinkUibcErr9[] = "input_category_list=none; generic_cap_list=Mouse; hidc_cap_list=none; port=none";
const char kSinkUibcErr10[] = "input_category_list=none; generic_cap_list=none, hidc_cap_list=none; port=none";
const char kSinkUibcErr11[] = "input_category_list=none; generic_cap_list=none; hydc_cap_list=none; port=none";
const char kSinkUibcErr12[] = "input_category_list=none; generic_cap_list=none; hidc_cap_list= none; port=none";
const char kSinkUibcErr13[] = "input_category_list=none; generic_cap_list=none; hidc_cap_list=Keyboard; port=none";
const char kSinkUibcErr14[] = "input_category_list=none; generic_cap_list=none; hidc_cap_list=none, port=none";
const char kSinkUibcErr15[] = "input_category_list=none; generic_cap_list=none; hidc_cap_list=none; port=nune";
const char kSinkUibcErr16[] = "input_category_list=GENERIG; generic_cap_list=none; hidc_cap_list=none; port=none";
const char kSinkUibcErr17[] = "input_category_list=GENERIC HIDC; generic_cap_list=none; hidc_cap_list=none; port=none";
const char kSinkUibcErr18[] = "input_category_list=GENERIC; generic_cap_list=Mous; hidc_cap_list=none; port=none";
const char kSinkUibcErr19[] = "input_category_list=GENERIC; generic_cap_list=Mouse keyboard; hidc_cap_list=none; port=none";
const char kSinkUibcErr20[] = "input_category_list=GENERIC; generic_cap_list=Mouse; Keyboard; hidc_cap_list=none; port=none";
const char kSinkUibcErr21[] = "input_category_list=GENERIC; generic_cap_list=Mouse, Keybuard; hidc_cap_list=none; port=none";
const char kSinkUibcErr22[] = "input_category_list=GENERIC; generic_cap_list=Mouse, Keyboard, SingleToch; hidc_cap_list=none; port=none";
const char kSinkUibcErr23[] = "input_category_list=GENERIC; generic_cap_list=MultiToach, Keyboard, SingleTouch; hidc_cap_list=none; port=none";
const char kSinkUibcErr24[] = "input_category_list=GENERIC; generic_cap_list=MultiTouch, Joystic, SingleTouch; hidc_cap_list=none; port=none";
const char kSinkUibcErr25[] = "input_category_list=GENERIC; generic_cap_list=MultiTouch, Joystick, Cumera; hidc_cap_list=none; port=none";
const char kSinkUibcErr26[] = "input_category_list=GENERIC; generic_cap_list=MultiTouch, Joystick, Camera, Gestue; hidc_cap_list=none; port=none";
const char kSinkUibcErr27[] = "input_category_list=GENERIC; generic_cap_list=MultiTouch, Joystick, Camera, Gesture, RemoteCantrol; hidc_cap_list=none; port=none";
const char kSinkUibcErr28[] = "input_category_list=HIDD; generic_cap_list=none; hidc_cap_list=none; port=none";
const char kSinkUibcErr29[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=none; port=none";
const char kSinkUibcErr30[] = "input_category_list=HIDC, GENERIC, generic_cap_list=none; hidc_cap_list=none; port=none";
const char kSinkUibcErr31[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=Mous; port=none";
const char kSinkUibcErr32[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=Mouse/Bt; port=none";
const char kSinkUibcErr33[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=Mouse/BT, port=none";
const char kSinkUibcErr34[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=Mouse/Infrared keyboard; port=none";
const char kSinkUibcErr35[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=Mouse/USB; Keyboard; port=none";
const char kSinkUibcErr36[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=Mouse/Zigbee, Keybuard/; port=none";
const char kSinkUibcErr37[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=Mouse/USB, Keyboard/ZigBee; port=none";
const char kSinkUibcErr38[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=Mouse/USB, KeyboardZigbee; port=none";
const char kSinkUibcErr39[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=Mouse/Wi-Fi, Keyboard/No-SP, SingleToche; port=none";
const char kSinkUibcErr40[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=Mouse/Wi-Fi, Keyboard/No-SP, SingleTouch/no-sp; port=none";
const char kSinkUibcErr41[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=MultiToach/Wi-Fi, Keyboard/BT, SingleTouch/BT; port=none";
const char kSinkUibcErr42[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=MultiTouch/BT, Joystic/Zigbee, SingleTouch/USB; port=none";
const char kSinkUibcErr43[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=MultiTouch/BT, Joystick/BT, Cumera/BT; port=none";
const char kSinkUibcErr44[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=MultiTouch/BT, Joystick/USB, Camera/USB, Gestue/USB; port=none";
const char kSinkUibcErr45[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=MultiTouch/BT, Joystick/BT, Camera/BT, Gesture/BT, RemoteCantrol/BT; port=none";
const char kSinkUibcErr46[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=MultiTouch/uSB, Joystick/BT, Camera/BT, Gesture/BT, RemoteControl/BT; port=none";
const char kSinkUibcErr47[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=MultiTouch/USB, Joystick/Infrarad, Camera/BT, Gesture/BT, RemoteControl/BT; port=none";
const char kSinkUibcErr48[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=MultiTouch/USB, Joystick/Infrared, Camera/Zigbea, Gesture/BT, RemoteControl/BT; port=none";
const char kSinkUibcErr49[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=MultiTouch/USB, Joystick/Infrared, Camera/Zigbee, Gesture/WiFi, RemoteControl/BT; port=none";
const char kSinkUibcErr50[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=MultiTouch/USB, Joystick/Infrared, Camera/Zigbee, Gesture/Wi-Fi, RemoteControl/nosp; port=none";
const char kSinkUibcErr51[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=Mouse/BT; port=123a";
const char kSinkUibcErr52[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=Mouse/BT; port=123;";
const char kSinkUibcErr53[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=Mouse/BT; port=abcd";

const char * kSinkUibcErr[] = {
    kSinkUibcErr1,  kSinkUibcErr2,  kSinkUibcErr3,  kSinkUibcErr4,
    kSinkUibcErr5,  kSinkUibcErr6,  kSinkUibcErr7,  kSinkUibcErr8,
    kSinkUibcErr9,  kSinkUibcErr10, kSinkUibcErr11, kSinkUibcErr12,
    kSinkUibcErr13, kSinkUibcErr14, kSinkUibcErr15, kSinkUibcErr16,
    kSinkUibcErr17, kSinkUibcErr18, kSinkUibcErr19, kSinkUibcErr20,
    kSinkUibcErr21, kSinkUibcErr22, kSinkUibcErr23, kSinkUibcErr24,
    kSinkUibcErr25, kSinkUibcErr26, kSinkUibcErr27, kSinkUibcErr28,
    kSinkUibcErr29, kSinkUibcErr30, kSinkUibcErr31, kSinkUibcErr32,
    kSinkUibcErr33, kSinkUibcErr34, kSinkUibcErr35, kSinkUibcErr36,
    kSinkUibcErr37, kSinkUibcErr38, kSinkUibcErr39, kSinkUibcErr40,
    kSinkUibcErr41, kSinkUibcErr42, kSinkUibcErr43, kSinkUibcErr44,
    kSinkUibcErr45, kSinkUibcErr46, kSinkUibcErr47, kSinkUibcErr48,
    kSinkUibcErr49, kSinkUibcErr50, kSinkUibcErr51, kSinkUibcErr52,
    kSinkUibcErr53,
};

const char kSinkUibcA[] = "input_category_list=GENERIC, HIDC; generic_cap_list=Mouse, Keyboard, SingleTouch, Camera; hidc_cap_list=Mouse/USB, RemoteControl/No-SP; port=none";

const char kApplyUibc1[] = "input_category_list=GENERIC; generic_cap_list=Mouse; hidc_cap_list=none; port=none";
const char kApplyUibc2[] = "input_category_list=GENERIC; generic_cap_list=Keyboard; hidc_cap_list=none; port=none";
const char kApplyUibc3[] = "input_category_list=GENERIC; generic_cap_list=SingleTouch; hidc_cap_list=none; port=none";
const char kApplyUibc4[] = "input_category_list=GENERIC; generic_cap_list=MultiTouch; hidc_cap_list=none; port=none";
const char kApplyUibc5[] = "input_category_list=GENERIC; generic_cap_list=Joystick; hidc_cap_list=none; port=none";
const char kApplyUibc6[] = "input_category_list=GENERIC; generic_cap_list=Camera; hidc_cap_list=none; port=none";
const char kApplyUibc7[] = "input_category_list=GENERIC; generic_cap_list=Gesture; hidc_cap_list=none; port=none";
const char kApplyUibc8[] = "input_category_list=GENERIC; generic_cap_list=RemoteControl; hidc_cap_list=none; port=none";
const char kApplyUibc9[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=Mouse/USB; port=none";
const char kApplyUibc10[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=Keyboard/USB; port=none";
const char kApplyUibc11[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=SingleTouch/BT; port=none";
const char kApplyUibc12[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=MultiTouch/Wi-Fi; port=none";
const char kApplyUibc13[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=Joystick/USB; port=none";
const char kApplyUibc14[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=Camera/Zigbee; port=none";
const char kApplyUibc15[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=Gesture/USB; port=none";
const char kApplyUibc16[] = "input_category_list=HIDC; generic_cap_list=none; hidc_cap_list=RemoteControl/No-SP; port=none";
const char kApplyUibc17[] = "input_category_list=GENERIC, HIDC; generic_cap_list=Mouse, SingleTouch; hidc_cap_list=Mouse/BT, RemoteControl/Wi-Fi; port=none";
const char kApplyUibc18[] = "input_category_list=GENERIC, HIDC; generic_cap_list=Mouse, SingleTouch; hidc_cap_list=Mouse/USB, RemoteControl/No-SP; port=none";
const char kApplyUibc19[] = "input_category_list=GENERIC, HIDC; generic_cap_list=Mouse, SingleTouch; hidc_cap_list=Mouse/USB; port=none";

const char * kApplyUibc[] = {
    kApplyUibc1,  kApplyUibc2,  kApplyUibc3,  kApplyUibc4,
    kApplyUibc5,  kApplyUibc6,  kApplyUibc7,  kApplyUibc8,
    kApplyUibc9,  kApplyUibc10, kApplyUibc11, kApplyUibc12,
    kApplyUibc13, kApplyUibc14, kApplyUibc15, kApplyUibc16,
    kApplyUibc17, kApplyUibc18, kApplyUibc19,
};

const char * kAppliedUibc[] = {
    kApplyUibc1,  kApplyUibc2,  kApplyUibc3,  NULL,
    NULL,         kApplyUibc6,  NULL,         NULL,
    kApplyUibc9,  NULL,         NULL,         NULL,
    NULL,         NULL,         NULL,         kApplyUibc16,
    NULL,         kApplyUibc18, kApplyUibc19,
};

const char kSourceUibc[] = "input_category_list=GENERIC, HIDC; generic_cap_list=Mouse, Keyboard, SingleTouch, MultiTouch; hidc_cap_list=Mouse/BT, Keyboard/Wi-Fi, RemoteControl/No-SP; port=1512";
const char kSelectUibc[] = "input_category_list=GENERIC, HIDC; generic_cap_list=Keyboard, Mouse, SingleTouch; hidc_cap_list=Mouse/USB, RemoteControl/No-SP; port=1512";

bool ParseGenerateVideo (const char * kSink) {
    sp<VideoParameters> vpSink = VideoParameters::parse(kSink);
    if (vpSink == NULL) {
        fprintf(stderr, "parse(kSink) method failed. See to log.\n");
        return false;
    }
    AString str = vpSink->generateVideoFormats();
    if (strcmp(str.c_str(), kSink) != 0) {
        fprintf(stderr, "generateVideoFormats() failed, and produced next line:"
                "\n\torig \"%s\"\n\tnew  \"%s\"\n", kSink, str.c_str());
        return false;
    }
    return true;
}

bool ParseGenerateAudio (const char * kSink) {
    sp<AudioParameters> apSink = AudioParameters::parse(kSink);
    if (apSink == NULL) {
        fprintf(stderr, "parse(kSink) method failed. See to log.\n");
        return false;
    }
    AString str = apSink->generateAudioFormats();
    if (strcmp(str.c_str(), kSink) != 0) {
        fprintf(stderr,
                "generateAudioFormats() failed, and produced next line:"
                "\n\torig \"%s\"\n\tnew  \"%s\"\n", kSink, str.c_str());
        return false;
    }
    return true;
}

bool ParseGenerateUibc (const char * kSink) {
    sp<UibcParameters> upSink = UibcParameters::parse(kSink);
    if (upSink == NULL) {
        fprintf(stderr, "UIBC parse(kSink) method failed. See to log.\n");
        return false;
    }
    AString str = upSink->generateUibcCapability();
    if (strcmp(str.c_str(), kSink) != 0) {
        fprintf(stderr,
                "generateUibcCapability() failed, and produced next line:"
                "\n\torig \"%s\"\n\tnew  \"%s\"\n", kSink, str.c_str());
        return false;
    }
    return true;
}

bool ApplyVideoMode (sp<VideoParameters> vpSink, const char * data) {
    sp<VideoMode> vm = vpSink->applyVideoMode(data);
    if (vm == NULL) {
        return false;
    }
    return true;
}

bool ApplyAudioMode (sp<AudioParameters> apSink, const char * data) {
    sp<AudioMode> am = apSink->applyAudioMode(data);
    if (am == NULL) {
        return false;
    }
    return true;
}

bool ApplyUibcParameters (sp<UibcParameters> upSink, const char * data, const char * ref) {
    sp<UibcParameters> upNew = upSink->applyUibcParameters(data);
    if (upNew == NULL) {
        if (ref != NULL) {
            return false;
        } else {
            return true;
        }
    }
    AString str = upNew->generateUibcCapability();
    if (strcmp(str.c_str(), ref) != 0) {
        fprintf(stderr, "applyUibcParameters() failed, and produced next line:"
                "\n\tref \"%s\"\n\tnew \"%s\"\n", ref, str.c_str());
        return false;
    }
    return true;
}

}  // namespace android

int main(int argc, char **argv) {
    using namespace android;

    //------------------------------------------------------------------
    //                         ElementaryParser
    //------------------------------------------------------------------
    int test = 1;

    {
        if (ElementaryParser::getBitIndex(0x10, 0xFF) != 4) {
            fprintf(stderr, "Test getBitIndex %04x failed\n", test);
            return 1;
        }

        test++;
        if (ElementaryParser::getBitIndex(0x00, 0xFFFF) != ElementaryParser::kErrNoBits) {
            fprintf(stderr, "Test getBitIndex %04x failed\n", test);
            return 1;
        }

        test++;
        if (ElementaryParser::getBitIndex(0x103500, 0x00FFFFFF) != ElementaryParser::kErrMultiBits) {
            fprintf(stderr, "Test getBitIndex %04x failed\n", test);
            return 1;
        }

        ElementaryParser ep(kElementary);

        uint32_t value;
        test++;
        if (!ep.parseHexBitField(2, 0x01, ElementaryParser::kSingleBit,
                ElementaryParser::kCommaSpace, &value) || value != 1) {
            ep.printError("Error in test line");
            fprintf(stderr, "Test parseHexBitField %04x failed\n", test);
            return 1;
        }

        test++;
        if (!ep.parseHexBitField(2, 0x03, ElementaryParser::kMultiBits,
                ElementaryParser::kSpace, &value) || value != 3) {
            ep.printError("Error in test line");
            fprintf(stderr, "Test parseHexBitFieldAndSpace %04x failed\n", test);
            return 1;
        }

        test++;
        if (ep.parseHexBitField(8, 0x0FFFFFFF, ElementaryParser::kSingleBitOrZero,
                ElementaryParser::kSemicolonSpace, &value)) {
            ep.printError("Error HEX bit field element in test line");
            fprintf(stderr, "Test parseHexBitFieldAndSpace %04x failed\n", test);
            return 1;
        }

        test++;
        if (ep.parseHexBitField(8, 0x758001A0, ElementaryParser::kSingleBit,
                ElementaryParser::kSemicolonSpace, &value)) {
            ep.printError("Error HEX bit field element in test line");
            fprintf(stderr, "Test parseHexBitFieldAndSpace %04x failed\n", test);
            return 1;
        }

        test++;
        if (!ep.parseHexBitField(8, 0x458001A0, ElementaryParser::kMultiBits,
                ElementaryParser::kSemicolonSpace, &value) || value != 0x458001A0) {
            ep.printError("Error in test line");
            fprintf(stderr, "Test parseHexBitFieldAndSpace %04x failed\n", test);
            return 1;
        }

        test++;
        if (!ep.parseStringField(kElementaryTable, ElementaryParser::kSlash, &value) || value != 1) {
            ep.printError("Error in test line");
            fprintf(stderr, "Test parseStringFieldAndSpace %04x failed\n", test);
            return 1;
        }

        test++;
        if (ep.parseStringField(kElementaryTable, ElementaryParser::kSpace, &value)) {
            ep.printError("Error text element in test line");
            fprintf(stderr, "Test parseStringFieldAndSpace %04x failed\n", test);
            return 1;
        }

        test++;
        if (!ep.checkStringField("Ac3", ElementaryParser::kEndOfLine)) {
            fprintf(stderr, "Test checkStringField %04x failed\n", test);
            return 1;
        }
    }


    //------------------------------------------------------------------
    //                           Video
    //------------------------------------------------------------------

    test = 0x10;

    if (!ParseGenerateVideo(kSink1)) {
        fprintf(stderr, "Test ParseGenerate %04x failed\n", test);
        return 1;
    }

    test++;
    if (!ParseGenerateVideo(kSink2)) {
        fprintf(stderr, "Test ParseGenerate %04x failed\n", test);
        return 1;
    }

    test = 0x20;
    for (uint i = 0; i < sizeof(kSinkErr)/sizeof(char*); i++, test++) {
        sp<VideoParameters> vpSink = VideoParameters::parse(kSinkErr[i]);
        if (vpSink == NULL) {
            continue;
        } else {
            fprintf(stderr, "Test parse %04x failed\n", test);
            return 1;
        }
    }

    {
        test = 0x50;
        sp<VideoParameters> vpSink = VideoParameters::parse(kSink2);
        for (uint i = 0; i < sizeof(kApplyVideoMode1)/sizeof(char*); i++, test++) {
            if (!ApplyVideoMode(vpSink, kApplyVideoMode1[i])) {
                fprintf(stderr, "Test applyVideoMode %04x failed\n", test);
                return 1;
            }
        }
    }

    {
        test = 0x60;
        sp<VideoParameters> vpSink = VideoParameters::parse(kSink2);
        {
            sp<VideoParameters> vpSource = VideoParameters::parse(kSource1);
            sp<VideoMode> vm = vpSource->getBestVideoMode(vpSink, NULL);
            if (vm == NULL) {
                fprintf(stderr, "Test getBestVideoMode %04x failed\n", test);
                return 1;
            }
            ALOGD("The best is %s", vm->toString().c_str());
        }
        test++;
        {
            sp<VideoParameters> vpSource = VideoParameters::parse(kSource2);
            sp<VideoMode> vm = vpSource->getBestVideoMode(vpSink, NULL);
            if (vm == NULL) {
                fprintf(stderr, "Test getBestVideoMode %04x failed\n", test);
                return 1;
            }
            ALOGD("The best is %s", vm->toString().c_str());
        }
        test++;
        {
            sp<VideoMode> vmDesired = new VideoMode();
            vmDesired->h264HighProfile = false;
            vmDesired->h264Level = OMX_VIDEO_AVCLevel31;
            vmDesired->width = 1680;
            vmDesired->height = 1024;
            vmDesired->frameRate = 60;
            sp<VideoParameters> vpSource = VideoParameters::parse(kSource3);
            sp<VideoMode> vm = vpSource->getBestVideoMode(vpSink, vmDesired);
            if (vm == NULL || !(*vm.get() == *vmDesired.get())) {
                fprintf(stderr, "Test getBestVideoMode %04x failed\n", test);
                return 1;
            }
            ALOGD("The best is %s", vm->toString().c_str());
            test++;
            AString str = vpSource->generateVideoMode(vmDesired);
            if (strcmp(str.c_str(), kB31x1680x1024x60)) {
                fprintf(stderr, "Test generateVideoMode %04x failed\n", test);
                fprintf(stderr, "Returned string \"%s\"\n", str.c_str());
                fprintf(stderr, "Waiting string \"%s\"\n", kB31x1680x1024x60);
                return 1;
            }
        }
    }

    //------------------------------------------------------------------
    //                           Audio
    //------------------------------------------------------------------

    test = 0x80;
    if (!ParseGenerateAudio(kSinkAudio1)) {
        fprintf(stderr, "Test ParseGenerate %04x failed\n", test);
        return 1;
    }
    test++;
    if (!ParseGenerateAudio(kSinkAudio2)) {
        fprintf(stderr, "Test ParseGenerate %04x failed\n", test);
        return 1;
    }

    test = 0x90;
    for (uint i = 0; i < sizeof(kSinkAudioErr) / sizeof(char*); i++, test++) {
        sp<AudioParameters> apSink = AudioParameters::parse(kSinkAudioErr[i]);
        if (apSink != NULL) {
            fprintf(stderr, "Test parse %04x failed\n", test);
            return 1;
        }
    }

    {
        test = 0xA0;
        sp<AudioParameters> apSink = AudioParameters::parse(kSinkAudio2);
        for (uint i = 0; i < sizeof(kApplyAudioMode1) / sizeof(char*); i++, test++) {
            if (!ApplyAudioMode(apSink, kApplyAudioMode1[i])) {
                fprintf(stderr, "Test applyAudioMode %04x failed\n", test);
                return 1;
            }
        }
    }

    {
        test = 0xB0;
        sp<AudioParameters> apSink = AudioParameters::parse(kSinkAudio2);
        {
            sp<AudioParameters> apSource = AudioParameters::parse(kSourceAudio1);
            sp<AudioMode> am = apSource->getBestAudioMode(apSink, NULL);
            if (am == NULL) {
                fprintf(stderr, "Test getBestAudioMode %04x failed\n", test);
                return 1;
            }
            ALOGD("The best is %s", am->toString().c_str());
        }
        test++;
        {
            sp<AudioParameters> apSource = AudioParameters::parse(kSourceAudio2);
            sp<AudioMode> am = apSource->getBestAudioMode(apSink, NULL);
            if (am == NULL) {
                fprintf(stderr, "Test getBestAudioMode %04x failed\n", test);
                return 1;
            }
            ALOGD("The best is %s", am->toString().c_str());
        }
        test++;
        {
            sp<AudioMode> amDesired = new AudioMode();
            amDesired->format = 1;
            amDesired->sampleRate = 48000;
            amDesired->sampleSize = 16;
            amDesired->channelNum = 4;
            sp<AudioParameters> apSource = AudioParameters::parse(kSourceAudio2);
            sp<AudioMode> am = apSource->getBestAudioMode(apSink, amDesired);
            if (am == NULL || !(*am.get() == *amDesired.get())) {
                fprintf(stderr, "Test getBestAudioMode %04x failed\n", test);
                return 1;
            }
            ALOGD("The best is %s", am->toString().c_str());
            test++;
            AString str = apSource->generateAudioMode(amDesired);
            if (strcmp(str.c_str(), kAac48000x16x4)) {
                fprintf(stderr, "Test generateVideoMode %04x failed\n", test);
                fprintf(stderr, "Returned king \"%s\"\n", str.c_str());
                fprintf(stderr, "Waiting king \"%s\"\n", kAac48000x16x4);
                return 1;
            }
        }

    }

    //------------------------------------------------------------------
    //                           UIBC
    //------------------------------------------------------------------

    test = 0x100;
    for (uint i = 0; i < sizeof(kSinkUibc)/sizeof(char*); i++, test++) {
        if (!ParseGenerateUibc(kSinkUibc[i])) {
            fprintf(stderr, "Test ParseGenerate %04x failed\n", test);
            return 1;
        }
    }

    test = 0x120;
    for (uint i = 0; i < sizeof(kSinkUibcErr)/sizeof(char*); i++, test++) {
        sp<UibcParameters> upSink = UibcParameters::parse(kSinkUibcErr[i]);
        if (upSink != NULL) {
            fprintf(stderr, "Test parse() %04x failed\n", test);
            return 1;
        }
    }

    {
        test = 0x180;
        sp<UibcParameters> upSink = UibcParameters::parse(kSinkUibcA);
        for (uint i = 0; i < sizeof(kApplyUibc) / sizeof(char*); i++, test++) {
            if (!ApplyUibcParameters(upSink, kApplyUibc[i], kAppliedUibc[i])) {
                fprintf(stderr, "Test applyUibcParameters %04x failed\n", test);
                return 1;
            }
        }
    }

    {
        test = 0x1A0;
        sp<UibcParameters> upSink = UibcParameters::parse(kSinkUibcA);
        sp<UibcParameters> upSource = UibcParameters::parse(kSourceUibc);
        sp<UibcParameters> upSelected = upSource->selectUibcParams(upSink);
        if (upSelected == NULL) {
            fprintf(stderr, "Test selectUibcParameters %04x failed\n", test);
            return 1;
        }
        AString str = upSelected->generateUibcCapability();
        if (strcmp(str.c_str(), kSelectUibc) != 0) {
            fprintf(stderr, "selectUibcParameters() failed, and produced next line:"
                    "\n\tref \"%s\"\n\tnew \"%s\"\n", kSelectUibc, str.c_str());
            return false;
        }
    }

    fprintf(stderr, "All tests passed\n");
    return 0;

}
