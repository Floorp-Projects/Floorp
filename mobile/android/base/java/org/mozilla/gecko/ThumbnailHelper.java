/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.mozglue.DirectBufferAllocator;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.util.Log;
import android.util.TypedValue;

import java.nio.ByteBuffer;
import java.util.LinkedList;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Helper class to generate thumbnails for tabs.
 * Internally, a queue of pending thumbnails is maintained in mPendingThumbnails.
 * The head of the queue is the thumbnail that is currently being processed; upon
 * completion of the current thumbnail the next one is automatically processed.
 * Changes to the thumbnail width are stashed in mPendingWidth and the change is
 * applied between thumbnail processing. This allows a single thumbnail buffer to
 * be used for all thumbnails.
 */
public final class ThumbnailHelper {
    private static final String LOGTAG = "GeckoThumbnailHelper";

    public static final float TABS_PANEL_THUMBNAIL_ASPECT_RATIO = 0.8333333f;
    public static final float TOP_SITES_THUMBNAIL_ASPECT_RATIO = 0.571428571f;  // this is a 4:7 ratio (as per UX decision)
    public static final float THUMBNAIL_ASPECT_RATIO;

    static {
      // As we only want to generate one thumbnail for each tab, we calculate the
      // largest aspect ratio required and create the thumbnail based off that.
      // Any views with a smaller aspect ratio will use a cropped version of the
      // same image.
      THUMBNAIL_ASPECT_RATIO = Math.max(TABS_PANEL_THUMBNAIL_ASPECT_RATIO, TOP_SITES_THUMBNAIL_ASPECT_RATIO);
    }

    public enum CachePolicy {
        STORE,
        NO_STORE
    }

    // static singleton stuff

    private static ThumbnailHelper sInstance;

    public static synchronized ThumbnailHelper getInstance() {
        if (sInstance == null) {
            sInstance = new ThumbnailHelper();
        }
        return sInstance;
    }

    // instance stuff

    private final LinkedList<Tab> mPendingThumbnails;    // synchronized access only
    private AtomicInteger mPendingWidth;
    private int mWidth;
    private int mHeight;
    private ByteBuffer mBuffer;

    private ThumbnailHelper() {
        final Resources res = GeckoAppShell.getContext().getResources();


        mPendingThumbnails = new LinkedList<Tab>();
        try {
            mPendingWidth = new AtomicInteger((int) res.getDimension(R.dimen.tab_thumbnail_width));
        } catch (Resources.NotFoundException nfe) { mPendingWidth = new AtomicInteger(0); }
        mWidth = -1;
        mHeight = -1;
    }

    public void getAndProcessThumbnailFor(Tab tab) {
        if (AboutPages.isAboutHome(tab.getURL()) || AboutPages.isAboutPrivateBrowsing(tab.getURL())) {
            tab.updateThumbnail(null, CachePolicy.NO_STORE);
            return;
        }

        synchronized (mPendingThumbnails) {
            if (mPendingThumbnails.lastIndexOf(tab) > 0) {
                // This tab is already in the queue, so don't add it again.
                // Note that if this tab is only at the *head* of the queue,
                // (i.e. mPendingThumbnails.lastIndexOf(tab) == 0) then we do
                // add it again because it may have already been thumbnailed
                // and now we need to do it again.
                return;
            }

            mPendingThumbnails.add(tab);
            if (mPendingThumbnails.size() > 1) {
                // Some thumbnail was already being processed, so wait
                // for that to be done.
                return;
            }
        }
        requestThumbnailFor(tab);
    }

    public void setThumbnailWidth(int width) {
        // Check inverted for safety: Bug 803299 Comment 34.
        if (GeckoAppShell.getScreenDepth() == 24) {
            mPendingWidth.set(width);
        } else {
            // Bug 776906: on 16-bit screens we need to ensure an even width.
            mPendingWidth.set((width & 1) == 0 ? width : width + 1);
        }
    }

    private void updateThumbnailSize() {
        // Apply any pending width updates.
        mWidth = mPendingWidth.get();
        mHeight = Math.round(mWidth * THUMBNAIL_ASPECT_RATIO);

        int pixelSize = (GeckoAppShell.getScreenDepth() == 24) ? 4 : 2;
        int capacity = mWidth * mHeight * pixelSize;
        Log.d(LOGTAG, "Using new thumbnail size: " + capacity + " (width " + mWidth + " - height " + mHeight + ")");
        if (mBuffer == null || mBuffer.capacity() != capacity) {
            if (mBuffer != null) {
                mBuffer = DirectBufferAllocator.free(mBuffer);
            }
            try {
                mBuffer = DirectBufferAllocator.allocate(capacity);
            } catch (IllegalArgumentException iae) {
                Log.w(LOGTAG, iae.toString());
            } catch (OutOfMemoryError oom) {
                Log.w(LOGTAG, "Unable to allocate thumbnail buffer of capacity " + capacity);
            }
            // If we hit an error above, mBuffer will be pointing to null, so we are in a sane state.
        }
    }

    private void requestThumbnailFor(Tab tab) {
        updateThumbnailSize();

        if (mBuffer == null) {
            // Buffer allocation may have failed. In this case we can't send the
            // event requesting the screenshot which means we won't get back a response
            // and so our queue will grow unboundedly. Handle this scenario by clearing
            // the queue (no point trying more thumbnailing right now since we're likely
            // low on memory). We will try again normally on the next call to
            // getAndProcessThumbnailFor which will hopefully be when we have more free memory.
            synchronized (mPendingThumbnails) {
                mPendingThumbnails.clear();
            }
            return;
        }

        Log.d(LOGTAG, "Sending thumbnail event: " + mWidth + ", " + mHeight);
        requestThumbnail(mBuffer, tab.getId(), mWidth, mHeight);
    }

    @WrapForJNI(dispatchTo = "proxy")
    private static native void requestThumbnail(ByteBuffer data, int tabId, int width, int height);

    /* This method is invoked by JNI once the thumbnail data is ready. */
    @WrapForJNI(stubName = "SendThumbnail", calledFrom = "gecko")
    public static void notifyThumbnail(ByteBuffer data, int tabId, boolean success, boolean shouldStore) {
        Tab tab = Tabs.getInstance().getTab(tabId);
        ThumbnailHelper helper = ThumbnailHelper.getInstance();
        if (success && tab != null) {
            helper.handleThumbnailData(tab, data, shouldStore ? CachePolicy.STORE : CachePolicy.NO_STORE);
        }
        helper.processNextThumbnail(tab);
    }

    private void processNextThumbnail(Tab tab) {
        Tab nextTab = null;
        synchronized (mPendingThumbnails) {
            if (tab != null && tab != mPendingThumbnails.peek()) {
                Log.e(LOGTAG, "handleThumbnailData called with unexpected tab's data!");
                // This should never happen, but recover gracefully by processing the
                // unexpected tab that we found in the queue
            } else {
                mPendingThumbnails.remove();
            }
            nextTab = mPendingThumbnails.peek();
        }
        if (nextTab != null) {
            requestThumbnailFor(nextTab);
        }
    }

    private void handleThumbnailData(Tab tab, ByteBuffer data, CachePolicy cachePolicy) {
        Log.d(LOGTAG, "handleThumbnailData: " + data.capacity());
        if (data != mBuffer) {
            // This should never happen, but log it and recover gracefully
            Log.e(LOGTAG, "handleThumbnailData called with an unexpected ByteBuffer!");
        }

        processThumbnailData(tab, data, cachePolicy);
    }

    private void processThumbnailData(Tab tab, ByteBuffer data, CachePolicy cachePolicy) {
        Bitmap b = tab.getThumbnailBitmap(mWidth, mHeight);
        data.position(0);
        b.copyPixelsFromBuffer(data);
        setTabThumbnail(tab, b, null, cachePolicy);
    }

    private void setTabThumbnail(Tab tab, Bitmap bitmap, byte[] compressed, CachePolicy cachePolicy) {
        if (bitmap == null) {
            if (compressed == null) {
                Log.w(LOGTAG, "setTabThumbnail: one of bitmap or compressed must be non-null!");
                return;
            }
            bitmap = BitmapUtils.decodeByteArray(compressed);
        }
        tab.updateThumbnail(bitmap, cachePolicy);
    }
}
