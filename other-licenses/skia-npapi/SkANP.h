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

#ifndef SkANP_DEFINED
#define SkANP_DEFINED

#include "android_npapi.h"
#include "SkCanvas.h"
#include "SkMatrix.h"
#include "SkPaint.h"
#include "SkPath.h"
#include "SkTypeface.h"

struct ANPMatrix : SkMatrix {
};

struct ANPPath : SkPath {
};

struct ANPPaint : SkPaint {
};

struct ANPTypeface : SkTypeface {
};

struct ANPCanvas {
    SkCanvas* skcanvas;

    // draw into the specified bitmap
    explicit ANPCanvas(const SkBitmap& bm) {
        skcanvas = new SkCanvas(bm);
    }

    // redirect all drawing to the specific SkCanvas
    explicit ANPCanvas(SkCanvas* other) {
        skcanvas = other;
    }

    ~ANPCanvas() {
        delete skcanvas;
    }
};

class SkANP {
public:
    static SkRect* SetRect(SkRect* dst, const ANPRectF& src);
    static SkIRect* SetRect(SkIRect* dst, const ANPRectI& src);
    static ANPRectI* SetRect(ANPRectI* dst, const SkIRect& src);
    static ANPRectF* SetRect(ANPRectF* dst, const SkRect& src);
    static SkBitmap* SetBitmap(SkBitmap* dst, const ANPBitmap& src);
    static bool SetBitmap(ANPBitmap* dst, const SkBitmap& src);
    
    static void InitEvent(ANPEvent* event, ANPEventType et);
};

#endif
