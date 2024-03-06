/* -*- Mode: Java; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.annotation.TargetApi;
import android.content.ClipData;
import android.content.ClipDescription;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Point;
import android.os.Build;
import android.text.TextUtils;
import android.util.Log;
import android.view.DragEvent;
import android.view.View;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import org.mozilla.gecko.annotation.WrapForJNI;

@TargetApi(Build.VERSION_CODES.N)
public class GeckoDragAndDrop {
  private static final String LOGTAG = "GeckoDragAndDrop";
  private static final boolean DEBUG = false;

  /** The drag/drop data is nsITransferable and stored into nsDragService. */
  private static final String MIMETYPE_NATIVE = "application/x-moz-draganddrop";

  private static final String[] sSupportedMimeType = {
    MIMETYPE_NATIVE, ClipDescription.MIMETYPE_TEXT_HTML, ClipDescription.MIMETYPE_TEXT_PLAIN
  };

  private static ClipData sDragClipData;
  private static float sX;
  private static float sY;
  private static boolean mEndingSession;

  private static class DrawDragImage extends View.DragShadowBuilder {
    private final Bitmap mBitmap;

    public DrawDragImage(final Bitmap bitmap) {
      if (bitmap != null && bitmap.getWidth() > 0 && bitmap.getHeight() > 0) {
        mBitmap = bitmap;
        return;
      }
      mBitmap = null;
    }

    @Override
    public void onProvideShadowMetrics(final Point outShadowSize, final Point outShadowTouchPoint) {
      if (mBitmap == null) {
        super.onProvideShadowMetrics(outShadowSize, outShadowTouchPoint);
        return;
      }
      outShadowSize.set(mBitmap.getWidth(), mBitmap.getHeight());
    }

    @Override
    public void onDrawShadow(final Canvas canvas) {
      if (mBitmap == null) {
        super.onDrawShadow(canvas);
        return;
      }
      canvas.drawBitmap(mBitmap, 0.0f, 0.0f, null);
    }
  }

  @WrapForJNI
  public static class DropData {
    public final String mimeType;
    public final String text;

    @WrapForJNI(skip = true)
    public DropData() {
      this.mimeType = MIMETYPE_NATIVE;
      this.text = null;
    }

    @WrapForJNI(skip = true)
    public DropData(final String mimeType) {
      this.mimeType = mimeType;
      this.text = "";
    }

    @WrapForJNI(skip = true)
    public DropData(final String mimeType, final String text) {
      this.mimeType = mimeType;
      this.text = text;
    }
  }

  public static void startDragAndDrop(final View view, final Bitmap bitmap) {
    view.startDragAndDrop(sDragClipData, new DrawDragImage(bitmap), null, View.DRAG_FLAG_GLOBAL);
    sDragClipData = null;
  }

  public static void updateDragImage(final View view, final Bitmap bitmap) {
    view.updateDragShadow(new DrawDragImage(bitmap));
  }

  public static boolean onDragEvent(@NonNull final DragEvent event) {
    if (DEBUG) {
      final StringBuilder sb = new StringBuilder("onDragEvent: action=");
      sb.append(event.getAction())
          .append(", x=")
          .append(event.getX())
          .append(", y=")
          .append(event.getY());
      Log.d(LOGTAG, sb.toString());
    }

    switch (event.getAction()) {
      case DragEvent.ACTION_DRAG_STARTED:
        mEndingSession = false;
        sX = event.getX();
        sY = event.getY();
        break;
      case DragEvent.ACTION_DRAG_LOCATION:
        sX = event.getX();
        sY = event.getY();
        break;
      case DragEvent.ACTION_DROP:
        sX = event.getX();
        sY = event.getY();
        break;
      case DragEvent.ACTION_DRAG_ENDED:
        mEndingSession = true;
        return true;
      default:
        break;
    }
    if (mEndingSession) {
      return false;
    }
    return true;
  }

  public static float getLocationX() {
    return sX;
  }

  public static float getLocationY() {
    return sY;
  }

  /**
   * Create drop data by DragEvent. This ClipData will be stored into nsDragService as
   * nsITransferable. If this type has MIMETYPE_NATIVE, this is already stored into nsDragService.
   * So do nothing.
   *
   * @param event A DragEvent
   * @return DropData that is from ClipData. If null, no data that we can convert to Gecko's type.
   */
  public static DropData createDropData(final DragEvent event) {
    final ClipDescription description = event.getClipDescription();

    if (event.getAction() == DragEvent.ACTION_DRAG_ENTERED) {
      // Android API cannot get real dragging item until drop event. So we set MIME type only.
      for (final String mimeType : sSupportedMimeType) {
        if (description.hasMimeType(mimeType)) {
          return new DropData(mimeType);
        }
      }
      return null;
    }

    if (event.getAction() != DragEvent.ACTION_DROP) {
      return null;
    }

    final ClipData clip = event.getClipData();
    if (clip == null || clip.getItemCount() == 0) {
      return null;
    }

    if (description.hasMimeType(MIMETYPE_NATIVE)) {
      if (DEBUG) {
        Log.d(LOGTAG, "Drop data is native nsITransferable. Do nothing");
      }
      return new DropData();
    }
    if (description.hasMimeType(ClipDescription.MIMETYPE_TEXT_HTML)) {
      final CharSequence data = clip.getItemAt(0).getHtmlText();
      if (data == null) {
        return null;
      }
      if (DEBUG) {
        Log.d(LOGTAG, "Drop data is text/html");
      }
      return new DropData(ClipDescription.MIMETYPE_TEXT_HTML, data.toString());
    }

    final CharSequence text = clip.getItemAt(0).coerceToText(GeckoAppShell.getApplicationContext());
    if (!TextUtils.isEmpty(text)) {
      if (DEBUG) {
        Log.d(LOGTAG, "Drop data is text/plain");
      }
      return new DropData(ClipDescription.MIMETYPE_TEXT_PLAIN, text.toString());
    }
    return null;
  }

  private static void setDragClipData(final ClipData clipData) {
    sDragClipData = clipData;
  }

  private static @Nullable ClipData getDragClipData() {
    return sDragClipData;
  }

  /**
   * Set drag item before calling View.startDragAndDrop. This is set from nsITransferable, so it
   * marks as native data.
   */
  @WrapForJNI
  private static void setDragData(final CharSequence text, final String htmlText) {
    if (TextUtils.isEmpty(text)) {
      final ClipDescription description =
          new ClipDescription("drag item", new String[] {MIMETYPE_NATIVE});
      final ClipData.Item item = new ClipData.Item("");
      final ClipData clipData = new ClipData(description, item);
      setDragClipData(clipData);
      return;
    }

    if (TextUtils.isEmpty(htmlText)) {
      final ClipDescription description =
          new ClipDescription(
              "drag item", new String[] {MIMETYPE_NATIVE, ClipDescription.MIMETYPE_TEXT_PLAIN});
      final ClipData.Item item = new ClipData.Item(text);
      final ClipData clipData = new ClipData(description, item);
      setDragClipData(clipData);
      return;
    }

    final ClipDescription description =
        new ClipDescription(
            "drag item",
            new String[] {
              MIMETYPE_NATIVE,
              ClipDescription.MIMETYPE_TEXT_HTML,
              ClipDescription.MIMETYPE_TEXT_PLAIN
            });
    final ClipData.Item item = new ClipData.Item(text, htmlText);
    final ClipData clipData = new ClipData(description, item);
    setDragClipData(clipData);
    return;
  }

  @WrapForJNI
  private static void endDragSession() {
    mEndingSession = true;
  }
}
