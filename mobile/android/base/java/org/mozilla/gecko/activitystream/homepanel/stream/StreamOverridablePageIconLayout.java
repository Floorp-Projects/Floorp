/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.homepanel.stream;

import android.content.Context;
import android.net.Uri;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import com.squareup.picasso.Callback;
import com.squareup.picasso.Picasso;
import org.mozilla.gecko.R;
import org.mozilla.gecko.icons.IconCallback;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.Icons;
import org.mozilla.gecko.util.NetworkUtils;
import org.mozilla.gecko.widget.FaviconView;

import java.lang.ref.WeakReference;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.Future;

/**
 * A layout that represents page icons in Activity Stream, which can be overridden with a custom URL.
 *
 * Under the hood, it switches between multiple icon views because favicons (in FaviconView)
 * are handled differently from other types of page images.
 *
 * An alternative implementation would create a flag to override FaviconView to handle non-favicon images but I
 * found it to be more complicated: all code added to FaviconView has to be aware of which state it's in. This
 * composable switcher layout abstracts the switching state from the FaviconView and keeps it simple, but will
 * use slightly more resources.
 */
public class StreamOverridablePageIconLayout extends FrameLayout implements IconCallback {

    private enum UIMode {
        FAVICON_IMAGE, NONFAVICON_IMAGE
    }

    private FaviconView faviconView;
    private ImageView imageView;

    private @Nullable Future<IconResponse> ongoingFaviconLoad;

    /**
     * A cache of URLs that Picasso has failed to load. Picasso will not cache which URLs it has failed to load so
     * this is used to prevent Picasso from making additional requests to failed URLs, which is useful when the
     * given URL does not contain an image.
     *
     * Picasso unfortunately does not implement this functionality: https://github.com/square/picasso/issues/475
     *
     * The consequences of not having highlight images and making requests each time the app is loaded are small,
     * so we keep this cache in memory only.
     *
     * HACK: this cache is static because it's messy to create a single instance of this cache and pass it to all
     * relevant instances at construction time. The downside of being static is that 1) the lifecycle of the cache
     * no longer related to the Activity and 2) *all* instances share the same cache. The original implementation
     * fixed these problems by overriding Activity.onCreateView and passing in a single instance to the cache there,
     * but it crashed on Android O.
     */
    private final static Set<String> nonFaviconFailedRequestURLs = Collections.synchronizedSet(new HashSet<String>());

    public StreamOverridablePageIconLayout(final Context context, final AttributeSet attrs) {
        super(context, attrs);
        LayoutInflater.from(context).inflate(R.layout.activity_stream_overridable_page_icon_layout, this, true);
        initViews();
    }

    /**
     * Updates the icon for the view. If a non-null overrideImageURL is provided, this image will be shown.
     * Otherwise, a favicon will be retrieved for the given pageURL.
     */
    public void updateIcon(@NonNull final String pageURL, @Nullable final String overrideImageURL) {
        cancelPendingRequests();

        // We don't know how the large the non-favicon images could be (bug 1388415) so for now we're only going
        // to download them on wifi. Alternatively, we could get these from the Gecko cache (see below).
        //
        // If the Picasso request will always fail (e.g. url does not contain an image), it will make the request each
        // time this method is called, which is each time the View is shown (or hidden and reshown): we prevent this by
        // checking against a cache of failed request urls. If we let Picasso make the request each time, this is bad
        // for the user's network and the replacement icon will pop-in after the timeout each time, which looks bad.
        if (NetworkUtils.isWifi(getContext()) &&
                !TextUtils.isEmpty(overrideImageURL) &&
                !nonFaviconFailedRequestURLs.contains(overrideImageURL)) {
            setUIMode(UIMode.NONFAVICON_IMAGE);

            // TODO (bug 1322501): Optimization: since we've already navigated to these pages, there's a chance
            // Gecko has the image in its cache: we should try to get it first before making this network request.
            Picasso.with(getContext())
                    .load(Uri.parse(overrideImageURL))
                    .fit()
                    .centerCrop()
                    .into(imageView, new OnErrorUsePageURLCallback(this, pageURL, overrideImageURL, nonFaviconFailedRequestURLs));
        } else {
            setFaviconImage(pageURL);
        }
    }

    private void setFaviconImage(@NonNull final String pageURL) {
        setUIMode(UIMode.FAVICON_IMAGE);
        ongoingFaviconLoad = Icons.with(getContext())
                .pageUrl(pageURL)
                .skipNetwork()
                .forActivityStream()
                .build()
                .execute(this);
    }

    @Override
    public void onIconResponse(final IconResponse response) {
        faviconView.updateImage(response);
    }

    private void setUIMode(final UIMode uiMode) {
        final View viewToShow;
        final View viewToHide;
        if (uiMode == UIMode.FAVICON_IMAGE) {
            viewToShow = faviconView;
            viewToHide = imageView;
        } else {
            viewToShow = imageView;
            viewToHide = faviconView;
        }

        viewToShow.setVisibility(View.VISIBLE);
        viewToHide.setVisibility(View.GONE);
    }


    private void cancelPendingRequests() {
        Picasso.with(getContext())
                .cancelRequest(imageView);

        if (ongoingFaviconLoad != null) {
            ongoingFaviconLoad.cancel(true);
            ongoingFaviconLoad = null;
        }
    }

    private void initViews() {
        faviconView = (FaviconView) findViewById(R.id.favicon_view);
        imageView = (ImageView) findViewById(R.id.image_view);
        setUIMode(UIMode.FAVICON_IMAGE); // set in code to ensure state is consistent.
    }

    private static class OnErrorUsePageURLCallback implements Callback {
        private final WeakReference<StreamOverridablePageIconLayout> layoutWeakReference;
        private final String pageURL;
        private final String requestURL;
        private final Set<String> failedRequestURLs;

        private OnErrorUsePageURLCallback(final StreamOverridablePageIconLayout layoutWeakReference,
                @NonNull final String pageURL,
                @NonNull final String requestURL,
                final Set<String> failedRequestURLs) {
            this.layoutWeakReference = new WeakReference<>(layoutWeakReference);
            this.pageURL = pageURL;
            this.requestURL = requestURL;
            this.failedRequestURLs = failedRequestURLs;
        }

        @Override
        public void onSuccess() { /* Picasso sets the image, nothing to do. */ }

        @Override
        public void onError() {
            // We currently don't distinguish between URLs that do not contain an image and
            // requests that failed for other reasons. However, these icons aren't vital
            // so it should be fine.
            failedRequestURLs.add(requestURL);

            // I'm slightly concerned that cancelPendingRequests could get called during
            // this Picasso -> Icons request chain and we'll get bugs where favicons don't
            // appear correctly. However, we're already in an unexpected error case so it's
            // probably not worth worrying about.
            final StreamOverridablePageIconLayout layout = layoutWeakReference.get();
            if (layout == null) { return; }
            layout.setFaviconImage(pageURL);
        }
    }
}