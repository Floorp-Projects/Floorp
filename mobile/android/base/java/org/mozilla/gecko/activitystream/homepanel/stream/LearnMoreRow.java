/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.homepanel.stream;

import android.support.annotation.LayoutRes;
import android.view.View;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.Locales;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.activitystream.ActivityStream;
import org.mozilla.gecko.activitystream.ActivityStreamTelemetry;

import java.util.Locale;

/**
 * A row in Activity Stream with a link to SUMO so that new users to Activity Stream can read about how it works.
 *
 * We added this in FF57, when we switched to the new top sites layout, because we were concerned users would be
 * confused by the new layout or not be able to find new features (e.g. how to disable Pocket). We may remove it
 * in a future release once users get used to it.
 *
 * Note: it's more correct to add this outside of the RecyclerView but we're trying to uplift this and it's a simpler
 * change to add it directly to the RecyclerView because we can follow the patterns already established for other row
 * items, rather than fighting with putting a RecyclerView (a scrollable container) inside a ScrollView (ouch) in
 * order to make sure Learn More is shown.
 */
public class LearnMoreRow extends StreamViewHolder {

    public static final @LayoutRes int LAYOUT_ID = R.layout.activity_stream_learn_more_row;

    /**
     * Template for the learn more link; the String params, in order, are:
     * - Firefox version
     * - OS
     * - locale
     */
    private static final String LEARN_MORE_URL_TEMPLATE = "https://support.mozilla.org/1/mobile/%s/%s/%s/activity-stream";

    public LearnMoreRow(final View itemView) {
        super(itemView);

        final View learnMoreLink = itemView.findViewById(R.id.learn_more_link);
        learnMoreLink.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(final View v) {
                Tabs.getInstance().loadUrl(getLearnMoreURL());
                sendOnClickTelemetry();
            }
        });
    }

    /**
     * Gets the URL to be opened when "Learn more" is clicked.
     *
     * The results of this method should not be cached because it retrieves a locale, which can change.
     */
    private static String getLearnMoreURL() {
        // I figured out which values to use here from GeckoPreferences.
        final String VERSION = AppConstants.MOZ_APP_VERSION;
        final String OS = AppConstants.OS_TARGET;
        final String LOCALE = Locales.getLanguageTag(Locale.getDefault());

        return String.format(LEARN_MORE_URL_TEMPLATE, VERSION, OS, LOCALE);
    }

    private static void sendOnClickTelemetry() {
        ActivityStreamTelemetry.Extras.Builder extras = ActivityStreamTelemetry.Extras.builder()
                .set(ActivityStreamTelemetry.Contract.SOURCE_TYPE, ActivityStreamTelemetry.Contract.TYPE_LEARN_MORE);

        Telemetry.sendUIEvent(
                TelemetryContract.Event.LOAD_URL,
                TelemetryContract.Method.LIST_ITEM,
                extras.build());
    }
}
