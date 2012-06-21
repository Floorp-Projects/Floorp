/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.videoengine;

import java.nio.ByteBuffer;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.SurfaceHolder.Callback;

public class ViESurfaceRenderer implements Callback {

    // the bitmap used for drawing.
    private Bitmap bitmap = null;
    private ByteBuffer byteBuffer;
    private SurfaceHolder surfaceHolder;
    // Rect of the source bitmap to draw
    private Rect srcRect = new Rect();
    // Rect of the destination canvas to draw to
    private Rect dstRect = new Rect();
    private int  dstHeight = 0;
    private int  dstWidth = 0;
    private float dstTopScale = 0;
    private float dstBottomScale = 1;
    private float dstLeftScale = 0;
    private float dstRightScale = 1;

    public  ViESurfaceRenderer(SurfaceView view) {
        surfaceHolder = view.getHolder();
        if(surfaceHolder == null)
            return;

        Canvas canvas = surfaceHolder.lockCanvas();
        if(canvas != null) {
            Rect dst =surfaceHolder.getSurfaceFrame();
            if(dst != null) {
                dstRect = dst;
                dstHeight =dstRect.bottom-dstRect.top;
                dstWidth = dstRect.right-dstRect.left;
            }
            surfaceHolder.unlockCanvasAndPost(canvas);
        }

        surfaceHolder.addCallback(this);
    }

    public void surfaceChanged(SurfaceHolder holder, int format,
            int in_width, int in_height) {

        dstHeight = in_height;
        dstWidth = in_width;
        dstRect.left = (int)(dstLeftScale*dstWidth);
        dstRect.top = (int)(dstTopScale*dstHeight);
        dstRect.bottom = (int)(dstBottomScale*dstHeight);
        dstRect.right = (int) (dstRightScale*dstWidth);
    }

    public void surfaceCreated(SurfaceHolder holder) {
        // TODO(leozwang) Auto-generated method stub
    }

    public void surfaceDestroyed(SurfaceHolder holder) {
        // TODO(leozwang) Auto-generated method stub
    }

    public Bitmap CreateBitmap(int width, int height) {
        if (bitmap == null) {
            try {
                android.os.Process.setThreadPriority(
                    android.os.Process.THREAD_PRIORITY_DISPLAY);
            }
            catch (Exception e) {
            }
        }
        bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.RGB_565);
        srcRect.left = 0;
        srcRect.top = 0;
        srcRect.bottom = height;
        srcRect.right = width;

        return bitmap;
    }

    public ByteBuffer CreateByteBuffer(int width, int height) {
        if (bitmap == null) {
            try {
                android.os.Process
                        .setThreadPriority(android.os.Process.THREAD_PRIORITY_DISPLAY);
            }
            catch (Exception e) {
            }
        }

        try {
            bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.RGB_565);
            byteBuffer = ByteBuffer.allocateDirect(width*height*2);
            srcRect.left = 0;
            srcRect.top = 0;
            srcRect.bottom = height;
            srcRect.right = width;
        }
        catch (Exception ex) {
            Log.e("*WEBRTC*", "Failed to CreateByteBuffer");
            bitmap = null;
            byteBuffer = null;
        }

        return byteBuffer;
    }

    public void SetCoordinates(float left, float top,
            float right, float bottom) {
        dstLeftScale = left;
        dstTopScale = top;
        dstRightScale = right;
        dstBottomScale = bottom;

        dstRect.left = (int)(dstLeftScale*dstWidth);
        dstRect.top = (int)(dstTopScale*dstHeight);
        dstRect.bottom = (int)(dstBottomScale*dstHeight);
        dstRect.right = (int) (dstRightScale*dstWidth);
    }

    public void DrawByteBuffer() {
        if(byteBuffer == null)
            return;
        byteBuffer.rewind();
        bitmap.copyPixelsFromBuffer(byteBuffer);
        DrawBitmap();
    }

    public void DrawBitmap() {
        if(bitmap == null)
            return;

        Canvas canvas = surfaceHolder.lockCanvas();
        if(canvas != null) {
            canvas.drawBitmap(bitmap, srcRect, dstRect, null);
            surfaceHolder.unlockCanvasAndPost(canvas);
        }
    }

}
