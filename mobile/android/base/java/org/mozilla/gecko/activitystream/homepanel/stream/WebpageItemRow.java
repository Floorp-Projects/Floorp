/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.homepanel.stream;

import android.content.Context;
import android.graphics.Color;
import android.support.annotation.NonNull;
import android.support.annotation.UiThread;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.activitystream.Utils;
import org.mozilla.gecko.activitystream.homepanel.model.WebpageRowModel;
import org.mozilla.gecko.reader.ReaderModeUtils;
import org.mozilla.gecko.util.DrawableUtil;
import org.mozilla.gecko.util.StringUtils;
import org.mozilla.gecko.util.TouchTargetUtil;
import org.mozilla.gecko.util.URIUtils;
import org.mozilla.gecko.util.ViewUtil;

import java.lang.ref.WeakReference;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.UUID;

public class WebpageItemRow extends StreamViewHolder implements Tabs.OnTabsChangedListener {
    private static final String LOGTAG = "GeckoWebpageItemRow";

    public static final int LAYOUT_ID = R.layout.activity_stream_webpage_item_row;
    private static final double SIZE_RATIO = 0.75;

    private WebpageRowModel webpageModel;
    private OnContentChangedListener contentChangedListener;
    private int position;
    private boolean switchToTabHintShown;

    private final StreamOverridablePageIconLayout pageIconLayout;
    private final TextView pageDomainView;
    private final TextView pageTitleView;
    private final ImageView pageSourceIconView;
    private final TextView pageSourceView;
    private final ImageView menuButton;
    private final TextView switchToTabHint;

    public WebpageItemRow(final View itemView,
                          final OnMenuButtonClickListener onMenuButtonClickListener,
                          final OnContentChangedListener contentChangedListener) {
        super(itemView);

        this.contentChangedListener = contentChangedListener;

        pageTitleView = (TextView) itemView.findViewById(R.id.page_title);
        pageIconLayout = (StreamOverridablePageIconLayout) itemView.findViewById(R.id.page_icon);
        pageSourceView = (TextView) itemView.findViewById(R.id.page_source);
        pageDomainView = (TextView) itemView.findViewById(R.id.page_domain);
        pageSourceIconView = (ImageView) itemView.findViewById(R.id.page_source_icon);
        switchToTabHint = (TextView) itemView.findViewById(R.id.switch_tab_hint);

        menuButton = (ImageView) itemView.findViewById(R.id.menu);
        menuButton.setImageDrawable(
                DrawableUtil.tintDrawable(menuButton.getContext(), R.drawable.menu, Color.LTGRAY));
        TouchTargetUtil.ensureTargetHitArea(menuButton, itemView);
        ViewUtil.enableTouchRipple(menuButton);
        menuButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (onMenuButtonClickListener != null) {
                    onMenuButtonClickListener.onMenuButtonClicked(WebpageItemRow.this, position);
                }
            }
        });
    }

    public void bind(WebpageRowModel model, int position, int tilesWidth) {
        this.webpageModel = model;
        this.position = position;

        final String backendHighlightTitle = model.getTitle();
        final String uiHighlightTitle = !TextUtils.isEmpty(backendHighlightTitle) ? backendHighlightTitle : model.getUrl();
        pageTitleView.setText(uiHighlightTitle);

        ViewGroup.LayoutParams layoutParams = pageIconLayout.getLayoutParams();
        layoutParams.width = tilesWidth;
        layoutParams.height = (int) Math.floor(tilesWidth * SIZE_RATIO);
        pageIconLayout.setLayoutParams(layoutParams);

        updateUiForSource(model.getSource());
        updatePageDomain();
        pageIconLayout.updateIcon(model.getUrl(), model.getImageUrl());

        switchToTabHintShown = isTabOpenedForItem();
        switchToTabHint.setVisibility(switchToTabHintShown ? View.VISIBLE : View.GONE);
    }

    public void initResources() {
        Tabs.registerOnTabsChangedListener(this);
    }

    public void doCleanup() {
        Tabs.unregisterOnTabsChangedListener(this);
    }

    public void updateUiForSource(Utils.HighlightSource source) {
        switch (source) {
            case BOOKMARKED:
                pageSourceView.setText(R.string.activity_stream_highlight_label_bookmarked);
                pageSourceView.setVisibility(View.VISIBLE);
                pageSourceIconView.setImageResource(R.drawable.ic_as_bookmarked);
                break;
            case VISITED:
                pageSourceView.setText(R.string.activity_stream_highlight_label_visited);
                pageSourceView.setVisibility(View.VISIBLE);
                pageSourceIconView.setImageResource(R.drawable.ic_as_visited);
                break;
            case POCKET:
                pageSourceView.setText(R.string.activity_stream_highlight_label_trending);
                pageSourceView.setVisibility(View.VISIBLE);
                pageSourceIconView.setImageResource(R.drawable.ic_as_trending);
                break;
            default:
                pageSourceView.setVisibility(View.INVISIBLE);
                pageSourceIconView.setImageResource(0);
                break;
        }
    }

    private void updatePageDomain() {
        final URI highlightURI;
        try {
            highlightURI = new URI(webpageModel.getUrl());
        } catch (final URISyntaxException e) {
            // If the URL is invalid, there's not much extra processing we can do on it.
            pageDomainView.setText(webpageModel.getUrl());
            return;
        }

        final UpdatePageDomainAsyncTask updatePageDomainTask = new UpdatePageDomainAsyncTask(itemView.getContext(),
                highlightURI, pageDomainView);
        updatePageDomainTask.execute();
    }

    public int getTileWidth() {
        return pageIconLayout.getWidth();
    }

    public int getTileHeight() {
        return pageIconLayout.getHeight();
    }

    /** Updates the text of the given view to the host second level domain. */
    private static class UpdatePageDomainAsyncTask extends URIUtils.GetFormattedDomainAsyncTask {
        private static final int VIEW_TAG_ID = R.id.page; // same as the view.

        private final WeakReference<TextView> pageDomainViewWeakReference;
        private final UUID viewTagAtStart;

        UpdatePageDomainAsyncTask(final Context contextReference, final URI uri, final TextView pageDomainView) {
            super(contextReference, uri, false, 1); // subdomain.domain.
            this.pageDomainViewWeakReference = new WeakReference<>(pageDomainView);

            // See isTagSameAsStartTag for details.
            viewTagAtStart = UUID.randomUUID();
            pageDomainView.setTag(VIEW_TAG_ID, viewTagAtStart);
        }

        @Override
        protected void onPostExecute(final String domainTitle) {
            super.onPostExecute(domainTitle);
            final TextView viewToUpdate = pageDomainViewWeakReference.get();

            if (viewToUpdate == null || !isTagSameAsStartTag(viewToUpdate)) {
                return;
            }

            // The title will be the empty String if the host cannot be determined. This is fine: it's very unlikely
            // and the page title will be URL if there's an error there so we wouldn't want to write the URL again here.
            viewToUpdate.setText(StringUtils.stripCommonSubdomains(domainTitle));
        }

        /**
         * Returns true if the tag on the given view matches the tag from the constructor. We do this to ensure
         * the View we're making this request for hasn't been re-used by the time this request completes.
         */
        @UiThread
        private boolean isTagSameAsStartTag(final View viewToCheck) {
            return viewTagAtStart.equals(viewToCheck.getTag(VIEW_TAG_ID));
        }
    }

    @Override
    public void onTabChanged(Tab tab, Tabs.TabEvents msg, String data) {
        final String itemUrl = webpageModel.getUrl();
        if (itemUrl == null || tab == null) {
            return;
        }

        // Only interested if a matching tab has been opened/closed/switched to a different page
        final boolean validEvent = msg.equals(Tabs.TabEvents.ADDED) ||
                msg.equals(Tabs.TabEvents.CLOSED) ||
                msg.equals(Tabs.TabEvents.LOCATION_CHANGE);
        if (!validEvent) {
            return;
        }

        // Actually check for any tab change regarding any of the currently displayed stream URLs
        //
        // "data" is an empty String for ADDED/CLOSED, and contains the previous/old URL during
        // LOCATION_CHANGE (the new URL is retrieved using tab.getURL()).
        //
        // Normalized tabUrl and data using "ReaderModeUtils.stripAboutReaderUrl"
        // because they can be about:reader URLs if the current or old tab page was a reader view
        final String tabUrl = ReaderModeUtils.stripAboutReaderUrl(tab.getURL());
        final String previousTabUrl = ReaderModeUtils.stripAboutReaderUrl(data);
        if ((itemUrl.equals(tabUrl) || itemUrl.equals(previousTabUrl)) &&
                (switchToTabHintShown != isTabOpenedForItem())) {
            switchToTabHintShown = !switchToTabHintShown;
            notifyListenerAboutContentChange();
        }
    }

    public View getTabletContextMenuAnchor() {
        return menuButton;
    }

    /**
     * @return true if there is an opened tab for this item's {@link #webpageModel} Url.
     */
    private boolean isTabOpenedForItem() {
        Tab tab = Tabs.getInstance().getFirstTabForUrl(webpageModel.getUrl(), isCurrentTabInPrivateMode());

        return tab != null;
    }

    /**
     * @return true if this view is shown inside a private tab, independent of whether
     * a private mode theme is applied via <code>setPrivateMode(true)</code>.
     */
    private boolean isCurrentTabInPrivateMode() {
        final Tab tab = Tabs.getInstance().getSelectedTab();
        return tab != null && tab.isPrivate();
    }

    private void notifyListenerAboutContentChange() {
        if (contentChangedListener != null) {
            contentChangedListener.onContentChanged(getAdapterPosition());
        }
    }

    public interface OnMenuButtonClickListener {
        void onMenuButtonClicked(WebpageItemRow row, int position);
    }

    /**
     * Informs upstream about changes regarding this row's content.
     */
    public interface OnContentChangedListener {
        void onContentChanged(int itemPosition);
    }
}
