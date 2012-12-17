/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.gfx.IntSize;
import org.mozilla.gecko.mozglue.DirectBufferAllocator;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.util.Log;

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
final class ThumbnailHelper {
    private static final String LOGTAG = "GeckoThumbnailHelper";

    public static final float THUMBNAIL_ASPECT_RATIO = 0.714f;  // this is a 5:7 ratio (as per UX decision)

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
        mPendingThumbnails = new LinkedList<Tab>();
        mPendingWidth = new AtomicInteger((int)GeckoApp.mAppContext.getResources().getDimension(R.dimen.tab_thumbnail_width));
        mWidth = -1;
        mHeight = -1;
    }

    public void getAndProcessThumbnailFor(Tab tab) {
        if ("about:home".equals(tab.getURL())) {
            tab.updateThumbnail(null);
            return;
        }

        if (tab.getState() == Tab.STATE_DELAYED) {
            String url = tab.getURL();
            if (url != null) {
                byte[] thumbnail = BrowserDB.getThumbnailForUrl(GeckoApp.mAppContext.getContentResolver(), url);
                if (thumbnail != null) {
                    setTabThumbnail(tab, null, thumbnail);
                }
            }
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
        mPendingWidth.set(IntSize.nextPowerOfTwo(width));
    }

    private void updateThumbnailSize() {
        // Apply any pending width updates
        mWidth = mPendingWidth.get();

        mWidth &= ~0x1; // Ensure the width is always an even number (bug 776906)
        mHeight = Math.round(mWidth * THUMBNAIL_ASPECT_RATIO);

        int capacity = mWidth * mHeight * 2; // Multiply by 2 for 16bpp
        if (mBuffer == null || mBuffer.capacity() != capacity) {
            if (mBuffer != null) {
                mBuffer = DirectBufferAllocator.free(mBuffer);
            }
            try {
                mBuffer = DirectBufferAllocator.allocate(capacity);
            } catch (OutOfMemoryError oom) {
                Log.w(LOGTAG, "Unable to allocate thumbnail buffer of capacity " + capacity);
                // At this point mBuffer will be pointing to null, so we are in a sane state.
            }
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

        GeckoEvent e = GeckoEvent.createThumbnailEvent(tab.getId(), mWidth, mHeight, mBuffer);
        GeckoAppShell.sendEventToGecko(e);
    }

    /* This method is invoked by JNI once the thumbnail data is ready. */
    public static void notifyThumbnail(ByteBuffer data, int tabId) {
        Tab tab = Tabs.getInstance().getTab(tabId);
        if (tab != null) {
            ThumbnailHelper.getInstance().handleThumbnailData(tab, data);
        }
    }

    private void handleThumbnailData(Tab tab, ByteBuffer data) {
        if (data != mBuffer) {
            // This should never happen, but log it and recover gracefully
            Log.e(LOGTAG, "handleThumbnailData called with an unexpected ByteBuffer!");
        }

        if (shouldUpdateThumbnail(tab)) {
            processThumbnailData(tab, data);
        }
        Tab nextTab = null;
        synchronized (mPendingThumbnails) {
            if (tab != mPendingThumbnails.peek()) {
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

    private void processThumbnailData(Tab tab, ByteBuffer data) {
        Bitmap b = tab.getThumbnailBitmap(mWidth, mHeight);
        data.position(0);
        b.copyPixelsFromBuffer(data);
        setTabThumbnail(tab, b, null);
    }

    private void setTabThumbnail(Tab tab, Bitmap bitmap, byte[] compressed) {
        try {
            if (bitmap == null) {
                if (compressed == null) {
                    Log.w(LOGTAG, "setTabThumbnail: one of bitmap or compressed must be non-null!");
                    return;
                }
                bitmap = BitmapFactory.decodeByteArray(compressed, 0, compressed.length);
            }
            tab.updateThumbnail(bitmap);
        } catch (OutOfMemoryError ome) {
            Log.w(LOGTAG, "setTabThumbnail: decoding byte array of length " + compressed.length + " ran out of memory");
        }
    }

    private boolean shouldUpdateThumbnail(Tab tab) {
        return (Tabs.getInstance().isSelectedTab(tab) || GeckoApp.mAppContext.areTabsShown());
    }
}
