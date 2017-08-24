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
import com.squareup.picasso.Picasso;
import org.mozilla.gecko.R;
import org.mozilla.gecko.icons.IconCallback;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.Icons;
import org.mozilla.gecko.util.NetworkUtils;
import org.mozilla.gecko.widget.FaviconView;

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
        if (NetworkUtils.isWifi(getContext()) &&
                !TextUtils.isEmpty(overrideImageURL)) {
            setUIMode(UIMode.NONFAVICON_IMAGE);

            // TODO (bug 1322501): Optimization: since we've already navigated to these pages, there's a chance
            // Gecko has the image in its cache: we should try to get it first before making this network request.
            Picasso.with(getContext())
                    .load(Uri.parse(overrideImageURL))
                    .fit()
                    .centerCrop()
                    .into(imageView);
        } else {
            setUIMode(UIMode.FAVICON_IMAGE);

            ongoingFaviconLoad = Icons.with(getContext())
                    .pageUrl(pageURL)
                    .skipNetwork()
                    .build()
                    .execute(this);
        }
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
}