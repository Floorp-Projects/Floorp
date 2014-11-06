/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import java.lang.ref.WeakReference;

import org.mozilla.gecko.R;
import org.mozilla.gecko.favicons.Favicons;
import org.mozilla.gecko.favicons.OnFaviconLoadedListener;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.widget.FaviconView;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Typeface;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.TextView;

public class TabHistoryItemRow extends RelativeLayout {
    private final FaviconView favicon;
    private final TextView title;
    private final ImageView timeLineTop;
    private final ImageView timeLineBottom;
    // Listener for handling Favicon loads.
    private final OnFaviconLoadedListener faviconListener;

    private int loadFaviconJobId = Favicons.NOT_LOADING;

    public TabHistoryItemRow(Context context, AttributeSet attrs) {
        super(context, attrs);
        LayoutInflater.from(context).inflate(R.layout.tab_history_item_row, this);
        favicon = (FaviconView) findViewById(R.id.tab_history_icon);
        title = (TextView) findViewById(R.id.tab_history_title);
        timeLineTop = (ImageView) findViewById(R.id.tab_history_timeline_top);
        timeLineBottom = (ImageView) findViewById(R.id.tab_history_timeline_bottom);
        faviconListener = new UpdateViewFaviconLoadedListener(favicon);
    }

    // Update the views with historic page detail.
    public void update(final TabHistoryPage historyPage, boolean isFirstElement, boolean isLastElement) {
        ThreadUtils.assertOnUiThread();

        timeLineTop.setVisibility(isFirstElement ? View.INVISIBLE : View.VISIBLE);
        timeLineBottom.setVisibility(isLastElement ? View.INVISIBLE : View.VISIBLE);
        title.setText(historyPage.getTitle());

        if (historyPage.isSelected()) {
            // Highlight title with bold font.
            title.setTypeface(null, Typeface.BOLD);
        } else {
            // Clear previously set bold font.
            title.setTypeface(null, Typeface.NORMAL);
        }

        favicon.setEnabled(historyPage.isSelected());
        favicon.clearImage();
        Favicons.cancelFaviconLoad(loadFaviconJobId);
        loadFaviconJobId = Favicons.getSizedFaviconForPageFromLocal(getContext(), historyPage.getUrl(), faviconListener);
    }

    // Only holds a reference to the FaviconView itself, so if the row gets
    // discarded while a task is outstanding, we'll leak less memory.
    private static class UpdateViewFaviconLoadedListener implements OnFaviconLoadedListener {
        private final WeakReference<FaviconView> view;
        public UpdateViewFaviconLoadedListener(FaviconView view) {
            this.view = new WeakReference<FaviconView>(view);
        }

        /**
         * Update this row's favicon.
         * <p>
         * This method is always invoked on the UI thread.
         */
        @Override
        public void onFaviconLoaded(String url, String faviconURL, Bitmap favicon) {
            FaviconView v = view.get();
            if (v == null) {
                return;
            }

            if (favicon == null) {
                v.showDefaultFavicon();
                return;
            }

            v.updateImage(favicon, faviconURL);
        }
    }
}
