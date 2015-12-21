/*
 * Copyright 2008, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// must include config.h first for webkit to fiddle with new/delete
#include "SkANP.h"
#include "SkFontHost.h"
#include "SkStream.h"
#include <stdlib.h>

static ANPTypeface* anp_createFromName(const char name[], ANPTypefaceStyle s) {
    SkTypeface* tf = SkTypeface::CreateFromName(name,
                                        static_cast<SkTypeface::Style>(s));
    return reinterpret_cast<ANPTypeface*>(tf);
}

static ANPTypeface* anp_createFromTypeface(const ANPTypeface* family,
                                           ANPTypefaceStyle s) {
    SkTypeface* tf = SkTypeface::CreateFromTypeface(family,
                                          static_cast<SkTypeface::Style>(s));
    return reinterpret_cast<ANPTypeface*>(tf);
}

static int32_t anp_getRefCount(const ANPTypeface* tf) {
    return tf ? tf->getRefCnt() : 0;
}

static void anp_ref(ANPTypeface* tf) {
    SkSafeRef(tf);
}

static void anp_unref(ANPTypeface* tf) {
    SkSafeUnref(tf);
}

static ANPTypefaceStyle anp_getStyle(const ANPTypeface* tf) {
    SkTypeface::Style s = tf ? tf->style() : SkTypeface::kNormal;
    return static_cast<ANPTypefaceStyle>(s);
}

static int32_t anp_getFontPath(const ANPTypeface* tf, char fileName[],
                               int32_t length, int32_t* index) {
    SkStream* stream = tf->openStream(index);

    return 0;
    /*
    if (stream->getFileName()) {
      strcpy(fileName, stream->getFileName());
    } else {
      return 0;
    }

    return strlen(fileName);
    */
}

static const char* gFontDir;
#define FONT_DIR_SUFFIX     "/fonts/"

static const char* anp_getFontDirectoryPath() {
    if (NULL == gFontDir) {
        const char* root = getenv("ANDROID_ROOT");
        size_t len = strlen(root);
        char* storage = (char*)malloc(len + sizeof(FONT_DIR_SUFFIX));
        if (NULL == storage) {
            return NULL;
        }
        memcpy(storage, root, len);
        memcpy(storage + len, FONT_DIR_SUFFIX, sizeof(FONT_DIR_SUFFIX));
        // save this assignment for last, so that if multiple threads call us
        // (which should never happen), we never return an incomplete global.
        // At worst, we would allocate storage for the path twice.
        gFontDir = storage;
    }
    return gFontDir;
}

///////////////////////////////////////////////////////////////////////////////

#define ASSIGN(obj, name)   (obj)->name = anp_##name

void InitTypeFaceInterface(ANPTypefaceInterfaceV0* i) {
    ASSIGN(i, createFromName);
    ASSIGN(i, createFromTypeface);
    ASSIGN(i, getRefCount);
    ASSIGN(i, ref);
    ASSIGN(i, unref);
    ASSIGN(i, getStyle);
    ASSIGN(i, getFontPath);
    ASSIGN(i, getFontDirectoryPath);
}
