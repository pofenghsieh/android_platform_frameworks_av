/*
 *****************************************************************************
 *
 *                                Android
 *                  ITTIAM SYSTEMS PVT LTD, BANGALORE
 *                           COPYRIGHT(C) 2010-20
 *
 *  This program  is  proprietary to  Ittiam  Systems  Private  Limited  and
 *  is protected under Indian  Copyright Law as an unpublished work. Its use
 *  and  disclosure  is  limited by  the terms  and  conditions of a license
 *  agreement. It may not be copied or otherwise  reproduced or disclosed to
 *  persons outside the licensee's organization except in accordance with the
 *  terms  and  conditions   of  such  an  agreement.  All  copies  and
 *  reproductions shall be the property of Ittiam Systems Private Limited and
 *  must bear this notice in its entirety.
 *
 *****************************************************************************
 */
/**
 *****************************************************************************
 *
 *  @file     ASFExtractor.cpp
 *
 *  @brief    This file works as a wrapper to the ASFExtractor class
 *
 *****************************************************************************
 */

#define LOG_TAG "ASFDummyExtractor"
#include <utils/Log.h>

#include <media/stagefright/MediaSource.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/MediaExtractor.h>
#include <utils/Vector.h>
#include <dlfcn.h>   /* For dynamic loading */
#include "include/ASFExtractor.h"

namespace android {
    static void * pASFHandle = NULL;

ASFExtractor::ASFExtractor(const sp<DataSource> &source) {
    const char *errstr;
    ALOGD("Dummy ASFExtractor contructor");

    pASFParser = new ASF_WRAPER;

    pASFParser->ASFExtractor =     ( ASFExtractorImpl* (*)(const android::sp<android::DataSource>&))dlsym(pASFHandle, "ASFExtractor");
    if((errstr = dlerror()) != NULL){
        ALOGE("dlsym(), err: %s", errstr);
        dlclose(pASFHandle);
        delete pASFParser;
        return;
    }
    pASFParser->destructorASFExtractor =     (void (*)(ASFExtractorImpl *))dlsym(pASFHandle, "destructorASFExtractor");
    if((errstr = dlerror()) != NULL){
        ALOGE("dlsym(), err: %s", errstr);
        dlclose(pASFHandle);
        delete pASFParser;
        return;
    }
    pASFParser->countTracks =       (size_t (*)(ASFExtractorImpl *))dlsym(pASFHandle, "countTracks");
    if((errstr = dlerror()) != NULL){
        ALOGE("dlsym(), err: %s", errstr);
        dlclose(pASFHandle);
        delete pASFParser;
        return;
    }
    pASFParser->getTrack =          (android::sp<android::MediaSource> (*)(size_t, ASFExtractorImpl *))dlsym(pASFHandle, "getTrack");
    if((errstr = dlerror()) != NULL){
        ALOGE("dlsym(), err: %s", errstr);
        dlclose(pASFHandle);
        delete pASFParser;
        return;
    }
    pASFParser->getTrackMetaData =  (android::sp<android::MetaData> (*)(size_t, uint32_t, ASFExtractorImpl *))dlsym(pASFHandle, "getTrackMetaData");
    if((errstr = dlerror()) != NULL){
        ALOGE("dlsym(), err: %s", errstr);
        dlclose(pASFHandle);
        delete pASFParser;
        return;
    }
    pASFParser->getMetaData =       (android::sp<android::MetaData> (*)(ASFExtractorImpl *))dlsym(pASFHandle, "getMetaData");
    if((errstr = dlerror()) != NULL){
        ALOGE("dlsym(), err: %s", errstr);
        dlclose(pASFHandle);
        delete pASFParser;
        return;
    }


    mHandle = (*pASFParser->ASFExtractor)(source);
}

ASFExtractor::~ASFExtractor() {
    ALOGD("Dummy ASFExtractor destructor");
    if(!pASFParser) {
        return;
    }

    (pASFParser->destructorASFExtractor)(mHandle);

    //DL lib is unloaded only when ref count drops to 0
    dlclose(pASFHandle);

    delete pASFParser;
    pASFParser = NULL;
}

size_t ASFExtractor::countTracks() {
    ALOGV("Dummy ASFExtractor::countTracks()");
    if(!pASFParser) {
        return 0;
    }

    return (*pASFParser->countTracks)(mHandle);
}

sp<MediaSource> ASFExtractor::getTrack(size_t index) {
    ALOGV("Dummy ASFExtractor::getTrack()");
    if(!pASFParser) {
        return NULL;
    }

    return (*pASFParser->getTrack)(index, mHandle);
}

sp<MetaData> ASFExtractor::getTrackMetaData(
        size_t index, uint32_t flags) {
    if(!pASFParser) {
        return NULL;
    }

    return (*pASFParser->getTrackMetaData)(index, flags, mHandle);
}

sp<MetaData> ASFExtractor::getMetaData() {
    ALOGV("Dummy ASFExtractor::getMetaData()");
    if(!pASFParser) {
        return NULL;
    }

    return (*pASFParser->getMetaData)(mHandle);
}

bool SniffASF(const sp<DataSource> &source,
              String8 *mimeType,
              float *confidence,
              sp<AMessage> *meta)
{
    const char *errstr;

    static bool (*pSniffASF)(
        const sp<DataSource> &source,
        String8 *mimeType,
        float *confidence,
        sp<AMessage> *meta);

    dlerror();

    pASFHandle = dlopen("/system/lib/libittiam_asfextractor.so", RTLD_LAZY);
    if((errstr = dlerror()) != NULL) {
        ALOGE("dlopen() err: %s", errstr);
        return false;
    }

    pSniffASF =(bool (*)(const android::sp<android::DataSource>&, android::String8*, float*, android::sp<android::AMessage>*)) dlsym(pASFHandle, "SniffASF");
    if((errstr = dlerror()) != NULL) {
        ALOGE("Error dlsym(pSniffASF), err: %s", errstr);
        return false;
    }
    bool asf = (*pSniffASF)(source, mimeType, confidence, meta);
    if (!asf) {
        //DL lib is unloaded only when ref count drops to 0
        dlclose(pASFHandle);
    }
    return asf;
}

bool isASFParserAvailable()
{
    FILE *pF;

    ALOGE ("isASFParserAvailable \n");
    pF = fopen("/system/lib/libittiam_asfextractor.so", "r");
    if(!pF) {
        ALOGW("ASF parser is not available");
        return false;
    }
    fclose(pF);

    return true;
}

}  // namespace android
