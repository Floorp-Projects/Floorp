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
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.fxa.activities.FxAccountGetStartedActivity;

public class WelcomePanel extends FirstrunPanel {
    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstance) {
        final ViewGroup root = (ViewGroup) inflater.inflate(R.layout.firstrun_welcome_fragment, container, false);
        root.findViewById(R.id.welcome_account).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.BUTTON, "firstrun-sync");

                final Intent accountIntent = new Intent(getActivity(), FxAccountGetStartedActivity.class);
                startActivity(accountIntent);

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
