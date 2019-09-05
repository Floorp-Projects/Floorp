/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.firstrun;

import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.activities.FxAccountWebFlowActivity;

/**
 * Base class for our first run pages. We call these FirstrunPanel for consistency
 * with HomePager/HomePanel.
 *
 * @see FirstrunPager for the containing pager.
 */
public class FirstrunPanel extends Fragment {

    protected boolean showBrowserHint = true;
    public static final String NO_MESSAGE = "";

    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container, Bundle savedInstance) {
        final ViewGroup root = (ViewGroup) inflater.inflate(R.layout.firstrun_basepanel_checkable_fragment, container, false);
        final Bundle args = getArguments();
        if (args != null) {
            final int image = args.getInt(FirstrunPagerConfig.KEY_IMAGE);
            final String message = args.getString(FirstrunPagerConfig.KEY_MESSAGE);
            final String subtext = args.getString(FirstrunPagerConfig.KEY_SUBTEXT);

            ((ImageView) root.findViewById(R.id.firstrun_image)).setImageDrawable(getResources().getDrawable(image));
            ((TextView) root.findViewById(R.id.firstrun_text)).setText(message);
            ((TextView) root.findViewById(R.id.firstrun_subtext)).setText(subtext);

            final TextView messageView = root.findViewById(R.id.firstrun_text);
            if (NO_MESSAGE.equals(message)) {
                messageView.setVisibility(View.GONE);
            } else {
                messageView.setText(message);
            }
        }

        root.findViewById(R.id.firstrun_link).setOnClickListener(v -> {
            Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.BUTTON, "firstrun-next");
            next();
        });

        root.findViewById(R.id.firstrun_account).setOnClickListener(v -> {
            Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.BUTTON, "firstrun-signup");
            showBrowserHint = false;

            final Intent intent = new Intent(FxAccountConstants.ACTION_FXA_GET_STARTED);
            intent.putExtra(FxAccountWebFlowActivity.EXTRA_ENDPOINT, FxAccountConstants.ENDPOINT_FIRSTRUN);
            intent.setFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
            startActivity(intent);

            close();
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
