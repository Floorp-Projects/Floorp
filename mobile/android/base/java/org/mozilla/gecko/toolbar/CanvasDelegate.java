/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import org.mozilla.gecko.AppConstants.Versions;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PorterDuff.Mode;
import android.graphics.PorterDuffXfermode;
import android.graphics.Shader;

class CanvasDelegate {
    Paint mPaint;
    PorterDuffXfermode mMode;
    DrawManager mDrawManager;

    // DrawManager would do a default draw of the background.
    static interface DrawManager {
        public void defaultDraw(Canvas canvas);
    }

    CanvasDelegate(DrawManager drawManager, Mode mode, Paint paint) {
        mDrawManager = drawManager;

        // DST_IN masks, DST_OUT clips.
        mMode = new PorterDuffXfermode(mode);

        mPaint = paint;
    }

    void draw(Canvas canvas, Path path, int width, int height) {
        Bitmap offscreen = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas offscreenCanvas = new Canvas(offscreen);

        // Do a default draw.
        mDrawManager.defaultDraw(offscreenCanvas);

        if (path != null && !path.isEmpty()) {
            // ICS added double-buffering, which made it easier for drawing the Path directly over the DST.
            // In pre-ICS, drawPath() doesn't seem to use ARGB_8888 mode for performance, hence transparency is not preserved.
            mPaint.setXfermode(mMode);
            offscreenCanvas.drawPath(path, mPaint);
        }

        offscreen.prepareToDraw();
        canvas.drawBitmap(offscreen, 0, 0, null);
    }

    void setShader(Shader shader) {
        mPaint.setShader(shader);
    }
}
