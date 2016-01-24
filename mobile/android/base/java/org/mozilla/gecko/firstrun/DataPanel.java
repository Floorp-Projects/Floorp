/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.firstrun;

import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;

public class DataPanel extends FirstrunPanel {
    private boolean isEnabled = false;

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstance) {
        final View root = super.onCreateView(inflater, container, savedInstance);
        final ImageView clickableImage = (ImageView) root.findViewById(R.id.firstrun_image);
        clickableImage.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                // Set new state.
                isEnabled = !isEnabled;
                int newResource = isEnabled ? R.drawable.firstrun_data_on : R.drawable.firstrun_data_off;
                ((ImageView) view).setImageResource(newResource);
                if (isEnabled) {
                    // Always block images.
                    PrefsHelper.setPref("browser.image_blocking", 0);
                } else {
                    // Default: always load images.
                    PrefsHelper.setPref("browser.image_blocking", 1);
                }
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.BUTTON, "firstrun-datasaving-" + isEnabled);
            }
        });

        return root;
    }
}
