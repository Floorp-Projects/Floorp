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
#include "SkRect.h"

static ANPCanvas* anp_newCanvas(const ANPBitmap* bitmap) {
    SkBitmap bm;
    return new ANPCanvas(*SkANP::SetBitmap(&bm, *bitmap));
}

static void anp_deleteCanvas(ANPCanvas* canvas) {
    delete canvas;
}

static void anp_save(ANPCanvas* canvas) {
    canvas->skcanvas->save();
}

static void anp_restore(ANPCanvas* canvas) {
    canvas->skcanvas->restore();
}

static void anp_translate(ANPCanvas* canvas, float tx, float ty) {
    canvas->skcanvas->translate(SkFloatToScalar(tx), SkFloatToScalar(ty));
}

static void anp_scale(ANPCanvas* canvas, float sx, float sy) {
    canvas->skcanvas->scale(SkFloatToScalar(sx), SkFloatToScalar(sy));
}

static void anp_rotate(ANPCanvas* canvas, float degrees) {
    canvas->skcanvas->rotate(SkFloatToScalar(degrees));
}

static void anp_skew(ANPCanvas* canvas, float kx, float ky) {
    canvas->skcanvas->skew(SkFloatToScalar(kx), SkFloatToScalar(ky));
}

static void anp_clipRect(ANPCanvas* canvas, const ANPRectF* rect) {
    SkRect r;
    canvas->skcanvas->clipRect(*SkANP::SetRect(&r, *rect));
}

static void anp_clipPath(ANPCanvas* canvas, const ANPPath* path) {
    canvas->skcanvas->clipPath(*path);
}
static void anp_concat(ANPCanvas* canvas, const ANPMatrix* matrix) {
    canvas->skcanvas->concat(*matrix);
}

static void anp_getTotalMatrix(ANPCanvas* canvas, ANPMatrix* matrix) {
    const SkMatrix& src = canvas->skcanvas->getTotalMatrix();
    *matrix = *reinterpret_cast<const ANPMatrix*>(&src);
}

static bool anp_getLocalClipBounds(ANPCanvas* canvas, ANPRectF* r,
                                   bool antialias) {
    SkRect bounds;
    if (canvas->skcanvas->getClipBounds(&bounds)) {
        SkANP::SetRect(r, bounds);
        return true;
    }
    return false;
}

static bool anp_getDeviceClipBounds(ANPCanvas* canvas, ANPRectI* r) {
    SkIRect bounds;
    if (canvas->skcanvas->getClipDeviceBounds(&bounds)) {
        SkANP::SetRect(r, bounds);
        return true;
    }
    return false;
}

static void anp_drawColor(ANPCanvas* canvas, ANPColor color) {
    canvas->skcanvas->drawColor(color);
}

static void anp_drawPaint(ANPCanvas* canvas, const ANPPaint* paint) {
    canvas->skcanvas->drawPaint(*paint);
}

static void anp_drawLine(ANPCanvas* canvas, float x0, float y0,
                         float x1, float y1, const ANPPaint* paint) {
    canvas->skcanvas->drawLine(SkFloatToScalar(x0), SkFloatToScalar(y0),
                           SkFloatToScalar(x1), SkFloatToScalar(y1), *paint);
}

static void anp_drawRect(ANPCanvas* canvas, const ANPRectF* rect,
                         const ANPPaint* paint) {
    SkRect  r;
    canvas->skcanvas->drawRect(*SkANP::SetRect(&r, *rect), *paint);
}

static void anp_drawOval(ANPCanvas* canvas, const ANPRectF* rect,
                         const ANPPaint* paint) {
    SkRect  r;
    canvas->skcanvas->drawOval(*SkANP::SetRect(&r, *rect), *paint);
}

static void anp_drawPath(ANPCanvas* canvas, const ANPPath* path,
                         const ANPPaint* paint) {
    canvas->skcanvas->drawPath(*path, *paint);
}

static void anp_drawText(ANPCanvas* canvas, const void* text, uint32_t length,
                         float x, float y, const ANPPaint* paint) {
    canvas->skcanvas->drawText(text, length,
                               SkFloatToScalar(x), SkFloatToScalar(y),
                               *paint);
}

static void anp_drawPosText(ANPCanvas* canvas, const void* text,
                uint32_t byteLength, const float xy[], const ANPPaint* paint) {
    canvas->skcanvas->drawPosText(text, byteLength,
                                  reinterpret_cast<const SkPoint*>(xy), *paint);
}

static void anp_drawBitmap(ANPCanvas* canvas, const ANPBitmap* bitmap,
                           float x, float y, const ANPPaint* paint) {
    SkBitmap    bm;
    canvas->skcanvas->drawBitmap(*SkANP::SetBitmap(&bm, *bitmap),
                                 SkFloatToScalar(x), SkFloatToScalar(y),
                                 paint);
}

static void anp_drawBitmapRect(ANPCanvas* canvas, const ANPBitmap* bitmap,
                              const ANPRectI* src, const ANPRectF* dst,
                               const ANPPaint* paint) {
    SkBitmap    bm;
    SkRect      dstR;
    SkIRect     srcR;

    if (src) {
        canvas->skcanvas->drawBitmapRect(*SkANP::SetBitmap(&bm, *bitmap),
                                         *SkANP::SetRect(&srcR, *src),
                                         *SkANP::SetRect(&dstR, *dst),
                                         paint);
    } else {
        canvas->skcanvas->drawBitmapRect(*SkANP::SetBitmap(&bm, *bitmap),
                                         *SkANP::SetRect(&dstR, *dst),
                                         paint);
    }
}

///////////////////////////////////////////////////////////////////////////////

#define ASSIGN(obj, name)   (obj)->name = anp_##name

void InitCanvasInterface(ANPCanvasInterfaceV0* i) {
    ASSIGN(i, newCanvas);
    ASSIGN(i, deleteCanvas);
    ASSIGN(i, save);
    ASSIGN(i, restore);
    ASSIGN(i, translate);
    ASSIGN(i, scale);
    ASSIGN(i, rotate);
    ASSIGN(i, skew);
    ASSIGN(i, clipRect);
    ASSIGN(i, clipPath);
    ASSIGN(i, concat);
    ASSIGN(i, getTotalMatrix);
    ASSIGN(i, getLocalClipBounds);
    ASSIGN(i, getDeviceClipBounds);
    ASSIGN(i, drawColor);
    ASSIGN(i, drawPaint);
    ASSIGN(i, drawLine);
    ASSIGN(i, drawRect);
    ASSIGN(i, drawOval);
    ASSIGN(i, drawPath);
    ASSIGN(i, drawText);
    ASSIGN(i, drawPosText);
    ASSIGN(i, drawBitmap);
    ASSIGN(i, drawBitmapRect);
}
