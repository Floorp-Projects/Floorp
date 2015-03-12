/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.menu;

import android.annotation.TargetApi;
import android.content.Context;
import android.util.AttributeSet;

import org.mozilla.gecko.R;

/**
 * A MenuItemActionView without the default child views.
 *
 * A better implementation would have the non-child view implementation as the parent of
 * MenuItemActionView, but this is simpler and faster to implement for something intended to be
 * uplifted, and this implementation will soon be replaced with the old implementation for adding
 * the share plane to the menu (see https://bug1122302.bugzilla.mozilla.org/attachment.cgi?id=8572126).
 */
public class QuickShareBarActionView extends MenuItemActionView {

    public QuickShareBarActionView(Context context, AttributeSet attrs) {
        this(context, attrs, R.attr.menuItemActionViewStyle);
    }

    @TargetApi(14)
    public QuickShareBarActionView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);

        // We remove the views so they are visible, but note that
        // the child still does some computation on them.
        removeAllViews();
    }
}
