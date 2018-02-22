/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.web;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.view.View;

import org.mozilla.focus.session.Session;

public interface IWebView {
    class HitTarget {
        public final boolean isLink;
        public final String linkURL;

        public final boolean isImage;
        public final String imageURL;

        public HitTarget(final boolean isLink, final String linkURL, final boolean isImage, final String imageURL) {
            if (isLink && linkURL == null) {
                throw new IllegalStateException("link hittarget must contain URL");
            }

            if (isImage && imageURL == null) {
                throw new IllegalStateException("image hittarget must contain URL");
            }

            this.isLink = isLink;
            this.linkURL = linkURL;
            this.isImage = isImage;
            this.imageURL = imageURL;
        }
    }

    interface Callback {
        void onPageStarted(String url);

        void onPageFinished(boolean isSecure);

        void onSecurityChanged(boolean isSecure, String host, String organization);

        void onProgress(int progress);

        void onURLChanged(final String url);

        void onRequest(final boolean isTriggeredByUserGesture);

        void onDownloadStart(Download download);

        void onLongPress(final HitTarget hitTarget);

        /**
         * Notify the host application that the current page has entered full screen mode.
         *
         * The callback needs to be invoked to request the page to exit full screen mode.
         *
         * Some IWebView implementations may pass a custom View which contains the web contents in
         * full screen mode.
         */
        void onEnterFullScreen(@NonNull  FullscreenCallback callback, @Nullable View view);

        /**
         * Notify the host application that the current page has exited full screen mode.
         *
         * If a View was passed when the application entered full screen mode then this view must
         * be hidden now.
         */
        void onExitFullScreen();

        void countBlockedTracker();

        void resetBlockedTrackers();

        void onBlockingStateChanged(boolean isBlockingEnabled);
    }

    interface FullscreenCallback {
        void fullScreenExited();
    }

    /**
     * Enable/Disable content blocking for this session (Only the blockers that are enabled in the app's settings will be turned on/off).
     */
    void setBlockingEnabled(boolean enabled);

    void setCallback(Callback callback);

    void onPause();

    void onResume();

    void destroy();

    void reload();

    void stopLoading();

    String getUrl();

    void loadUrl(String url);

    void cleanup();

    void goForward();

    void goBack();

    boolean canGoForward();

    boolean canGoBack();

    void restoreWebViewState(Session session);

    void saveWebViewState(@NonNull Session session);

    void exitFullscreen();

    /**
     * Get the title of the currently displayed website.
     */
    String getTitle();
}