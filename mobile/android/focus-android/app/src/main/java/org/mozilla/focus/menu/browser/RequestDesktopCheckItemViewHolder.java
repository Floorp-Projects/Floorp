/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.menu.browser;

import android.view.View;
import android.widget.CheckBox;
import android.widget.CompoundButton;

import org.mozilla.focus.R;
import org.mozilla.focus.fragment.BrowserFragment;
import org.mozilla.focus.telemetry.TelemetryWrapper;
import org.mozilla.focus.utils.UrlUtils;

import mozilla.components.support.utils.ThreadUtils;

/* package-private */ class RequestDesktopCheckItemViewHolder extends BrowserMenuViewHolder implements
        CheckBox.OnCheckedChangeListener {
    /* package-private */ static final int LAYOUT_ID = R.layout.request_desktop_check_menu_item;

    private final BrowserFragment fragment;
    private CheckBox checkbox;

    /* package */ RequestDesktopCheckItemViewHolder(View itemView, final BrowserFragment fragment) {
        super(itemView);

        this.fragment = fragment;

        checkbox = itemView.findViewById(R.id.check_menu_item_checkbox);
        checkbox.setChecked(fragment.getSession().shouldRequestDesktopSite());
        checkbox.setOnCheckedChangeListener(this);
    }

    @Override
    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
        fragment.setShouldRequestDesktop(isChecked);
        TelemetryWrapper.desktopRequestCheckEvent(isChecked);

        // Delay closing the menu and reloading the website a bit so that the user can actually see
        // the switch change its state.
        ThreadUtils.INSTANCE.postToMainThreadDelayed(new Runnable() {
            @Override
            public void run() {
                getMenu().dismiss();
                fragment.loadUrl(UrlUtils.stripSchemeAndSubDomain(fragment.getUrl()));
            }
        }, /* Switch.THUMB_ANIMATION_DURATION */ 250);
    }
}
