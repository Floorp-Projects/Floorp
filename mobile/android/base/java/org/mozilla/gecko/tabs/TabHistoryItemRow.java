/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import android.content.Context;
import android.graphics.Typeface;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.TextView;

import org.mozilla.gecko.R;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.Icons;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.widget.FaviconView;

import java.util.concurrent.Future;

public class TabHistoryItemRow extends RelativeLayout {
    private final FaviconView favicon;
    private final TextView title;
    private final ImageView timeLineTop;
    private final ImageView timeLineBottom;
    private Future<IconResponse> ongoingIconLoad;

    public TabHistoryItemRow(Context context, AttributeSet attrs) {
        super(context, attrs);
        LayoutInflater.from(context).inflate(R.layout.tab_history_item_row, this);
        favicon = (FaviconView) findViewById(R.id.tab_history_icon);
        title = (TextView) findViewById(R.id.tab_history_title);
        timeLineTop = (ImageView) findViewById(R.id.tab_history_timeline_top);
        timeLineBottom = (ImageView) findViewById(R.id.tab_history_timeline_bottom);
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

        if (ongoingIconLoad != null) {
            ongoingIconLoad.cancel(true);
        }

        ongoingIconLoad = Icons.with(getContext())
                .pageUrl(historyPage.getUrl())
                .skipNetwork()
                .build()
                .execute(favicon.createIconCallback());
    }
}
