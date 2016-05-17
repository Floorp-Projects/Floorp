/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.firstrun;

import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;

/**
 * Base class for our first run pages. We call these FirstrunPanel for consistency
 * with HomePager/HomePanel.
 *
 * @see FirstrunPager for the containing pager.
 */
public class FirstrunPanel extends Fragment {

    public static final int TITLE_RES = -1;
    protected boolean showBrowserHint = true;

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstance) {
        final ViewGroup root = (ViewGroup) inflater.inflate(R.layout.firstrun_basepanel_checkable_fragment, container, false);
        Bundle args = getArguments();
        if (args != null) {
            final int imageRes = args.getInt(FirstrunPagerConfig.KEY_IMAGE);
            final int textRes = args.getInt(FirstrunPagerConfig.KEY_TEXT);
            final int subtextRes = args.getInt(FirstrunPagerConfig.KEY_SUBTEXT);

            ((ImageView) root.findViewById(R.id.firstrun_image)).setImageResource(imageRes);
            ((TextView) root.findViewById(R.id.firstrun_text)).setText(textRes);
            ((TextView) root.findViewById(R.id.firstrun_subtext)).setText(subtextRes);
        }

        root.findViewById(R.id.firstrun_link).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.BUTTON, "firstrun-next");
                pagerNavigation.next();
            }
        });

        return root;
    }

    public interface PagerNavigation {
        void next();
        void finish();
    }
    protected PagerNavigation pagerNavigation;

    public void setPagerNavigation(PagerNavigation listener) {
        this.pagerNavigation = listener;
    }

    protected void next() {
        if (pagerNavigation != null) {
            pagerNavigation.next();
        }
    }

    protected void close() {
        if (pagerNavigation != null) {
            pagerNavigation.finish();
        }
    }

    protected boolean shouldShowBrowserHint() {
        return showBrowserHint;
    }
}
