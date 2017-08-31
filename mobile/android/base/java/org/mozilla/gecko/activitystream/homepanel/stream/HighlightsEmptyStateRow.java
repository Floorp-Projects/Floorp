/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.homepanel.stream;

import android.support.annotation.LayoutRes;
import android.view.View;
import org.mozilla.gecko.R;

/** A row to be displayed when there are no highlights. */
public class HighlightsEmptyStateRow extends StreamViewHolder {

    @LayoutRes
    public static final int LAYOUT_ID = R.layout.activity_stream_highlights_empty_state;

    public HighlightsEmptyStateRow(final View itemView) {
        super(itemView);
    }
}
