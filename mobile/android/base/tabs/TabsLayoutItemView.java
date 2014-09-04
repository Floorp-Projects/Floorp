/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.widget.TabThumbnailWrapper;

import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.TextView;

public class TabsLayoutItemView {
    int id;
    TextView title;
    ImageView thumbnail;
    ImageButton close;
    ViewGroup info;
    TabThumbnailWrapper thumbnailWrapper;

    public TabsLayoutItemView(View view) {
        info = (ViewGroup) view;
        title = (TextView) view.findViewById(R.id.title);
        thumbnail = (ImageView) view.findViewById(R.id.thumbnail);
        close = (ImageButton) view.findViewById(R.id.close);
        thumbnailWrapper = (TabThumbnailWrapper) view.findViewById(R.id.wrapper);
    }

    protected void assignValues(Tab tab) {
        if (tab == null)
            return;

        id = tab.getId();

        Drawable thumbnailImage = tab.getThumbnail();
        if (thumbnailImage != null) {
            thumbnail.setImageDrawable(thumbnailImage);
        } else {
            thumbnail.setImageResource(R.drawable.tab_thumbnail_default);
        }
        if (thumbnailWrapper != null) {
            thumbnailWrapper.setRecording(tab.isRecording());
        }
        title.setText(tab.getDisplayTitle());
        close.setTag(this);
    }
}
