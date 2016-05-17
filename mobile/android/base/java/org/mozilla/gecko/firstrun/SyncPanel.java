/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.firstrun;

import android.content.Intent;
import android.os.Bundle;
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

public class SyncPanel extends FirstrunPanel {
    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstance) {
        final ViewGroup root = (ViewGroup) inflater.inflate(R.layout.firstrun_sync_fragment, container, false);
        final Bundle args = getArguments();
        if (args != null) {
            final int imageRes = args.getInt(FirstrunPagerConfig.KEY_IMAGE);
            final int textRes = args.getInt(FirstrunPagerConfig.KEY_TEXT);
            final int subtextRes = args.getInt(FirstrunPagerConfig.KEY_SUBTEXT);

            ((ImageView) root.findViewById(R.id.firstrun_image)).setImageResource(imageRes);
            ((TextView) root.findViewById(R.id.firstrun_text)).setText(textRes);
            ((TextView) root.findViewById(R.id.welcome_browse)).setText(subtextRes);
        }

        root.findViewById(R.id.welcome_account).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.BUTTON, "firstrun-sync");
                showBrowserHint = false;

                final Intent intent = new Intent(FxAccountConstants.ACTION_FXA_GET_STARTED);
                intent.putExtra(FxAccountWebFlowActivity.EXTRA_ENDPOINT, FxAccountConstants.ENDPOINT_FIRSTRUN);
                intent.setFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
                startActivity(intent);

                close();
            }
        });

        root.findViewById(R.id.welcome_browse).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.BUTTON, "firstrun-browser");
                close();
            }
        });

        return root;
    }
}
