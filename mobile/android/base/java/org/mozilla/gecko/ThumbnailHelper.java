/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.util.ResourceDrawableUtils;
import org.mozilla.gecko.mozglue.DirectBufferAllocator;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.util.Log;

import java.nio.ByteBuffer;
import java.util.ArrayList;

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
    private static final boolean DEBUG = false;
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

    private final ArrayList<Tab> mPendingThumbnails;    // synchronized access only
    private volatile int mPendingWidth;
    private int mWidth;
    private int mHeight;
    private ByteBuffer mBuffer;

    private ThumbnailHelper() {
        final Resources res = GeckoAppShell.getApplicationContext().getResources();

        mPendingThumbnails = new ArrayList<>();
        try {
            mPendingWidth = (int) res.getDimension(R.dimen.tab_thumbnail_width);
        } catch (Resources.NotFoundException nfe) {
        }
        mWidth = -1;
        mHeight = -1;
    }

    public void getAndProcessThumbnailFor(final int tabId, final ResourceDrawableUtils.BitmapLoader loader) {
        final Tab tab = Tabs.getInstance().getTab(tabId);
        if (tab != null) {
            getAndProcessThumbnailFor(tab, loader);
        }
    }

    public void getAndProcessThumbnailFor(final Tab tab, final ResourceDrawableUtils.BitmapLoader loader) {
        ResourceDrawableUtils.runOnBitmapFoundOnUiThread(loader, tab.getThumbnail());

        Tabs.registerOnTabsChangedListener(new Tabs.OnTabsChangedListener() {
                @Override
                public void onTabChanged(final Tab t, final Tabs.TabEvents msg, final String data) {
                    if (tab != t || msg != Tabs.TabEvents.THUMBNAIL) {
                        return;
                    }
                    Tabs.unregisterOnTabsChangedListener(this);
                    ResourceDrawableUtils.runOnBitmapFoundOnUiThread(loader, t.getThumbnail());
                }
            });
        getAndProcessThumbnailFor(tab);
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

            requestThumbnailLocked(tab);
        }
    }

    public void setThumbnailWidth(int width) {
        // Check inverted for safety: Bug 803299 Comment 34.
        if (GeckoAppShell.getScreenDepth() == 24) {
            mPendingWidth = width;
        } else {
            // Bug 776906: on 16-bit screens we need to ensure an even width.
            mPendingWidth = (width + 1) & (~1);
        }
    }

    private void updateThumbnailSizeLocked() {
        // Apply any pending width updates.
        mWidth = mPendingWidth;
        mHeight = Math.round(mWidth * THUMBNAIL_ASPECT_RATIO);

        int pixelSize = (GeckoAppShell.getScreenDepth() == 24) ? 4 : 2;
        int capacity = mWidth * mHeight * pixelSize;
        if (DEBUG) {
            Log.d(LOGTAG, "Using new thumbnail size: " + capacity +
                          " (width " + mWidth + " - height " + mHeight + ")");
        }
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

    private void requestThumbnailLocked(Tab tab) {
        updateThumbnailSizeLocked();

        if (mBuffer == null) {
            // Buffer allocation may have failed. In this case we can't send the
            // event requesting the screenshot which means we won't get back a response
            // and so our queue will grow unboundedly. Handle this scenario by clearing
            // the queue (no point trying more thumbnailing right now since we're likely
            // low on memory). We will try again normally on the next call to
            // getAndProcessThumbnailFor which will hopefully be when we have more free memory.
            mPendingThumbnails.clear();
            return;
        }

        if (DEBUG) {
            Log.d(LOGTAG, "Sending thumbnail event: " + mWidth + ", " + mHeight);
        }
        requestThumbnailLocked(mBuffer, tab, tab.getId(), mWidth, mHeight);
    }

    @WrapForJNI(stubName = "RequestThumbnail", dispatchTo = "proxy")
    private static native void requestThumbnailLocked(ByteBuffer data, Tab tab, int tabId,
                                                      int width, int height);

    /* This method is invoked by JNI once the thumbnail data is ready. */
    @WrapForJNI(calledFrom = "gecko")
    private static void notifyThumbnail(final ByteBuffer data, final Tab tab,
                                        final boolean success, final boolean shouldStore) {
        final ThumbnailHelper helper = ThumbnailHelper.getInstance();
        if (success) {
            helper.handleThumbnailData(
                    tab, data, shouldStore ? CachePolicy.STORE : CachePolicy.NO_STORE);
        }
        helper.processNextThumbnail();
    }

    private void processNextThumbnail() {
        synchronized (mPendingThumbnails) {
            if (mPendingThumbnails.isEmpty()) {
                return;
            }

            mPendingThumbnails.remove(0);

            if (!mPendingThumbnails.isEmpty()) {
                requestThumbnailLocked(mPendingThumbnails.get(0));
            }
        }
    }

    private void handleThumbnailData(Tab tab, ByteBuffer data, CachePolicy cachePolicy) {
        if (DEBUG) {
            Log.d(LOGTAG, "handleThumbnailData: " + data.capacity());
        }
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
