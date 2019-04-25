/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.firstrun;

import android.graphics.Bitmap;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;

public class LastPanel extends FirstrunPanel {
    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstance) {
        final ViewGroup root = (ViewGroup) inflater.inflate(R.layout.firstrun_basepanel_checkable_fragment, container, false);
        final Bundle args = getArguments();
        if (args != null) {
            final int image = args.getInt(FirstrunPagerConfig.KEY_IMAGE);
            final String message = args.getString(FirstrunPagerConfig.KEY_MESSAGE);
            final String subtext = args.getString(FirstrunPagerConfig.KEY_SUBTEXT);

            ((ImageView) root.findViewById(R.id.firstrun_image)).setImageDrawable(getResources().getDrawable(image));
            ((TextView) root.findViewById(R.id.firstrun_subtext)).setText(subtext);
            ((TextView) root.findViewById(R.id.firstrun_link)).setText(R.string.firstrun_welcome_button_browser);

            final TextView messageView = root.findViewById(R.id.firstrun_text);
            if (NO_MESSAGE.equals(message)) {
                messageView.setVisibility(View.GONE);
            } else {
                messageView.setText(message);
            }
        }

        root.findViewById(R.id.firstrun_link).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.BUTTON, "firstrun-next");
                close();
            }
        });

        return root;
    }
}
