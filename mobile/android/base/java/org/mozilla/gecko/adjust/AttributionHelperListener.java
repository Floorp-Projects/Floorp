/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.adjust;

/**
 * Because of how our build module dependencies are structured, we aren't able to use
 * the {@link com.adjust.sdk.OnAttributionChangedListener} directly outside of {@link AdjustHelper}.
 * If the Adjust SDK is enabled, this listener should be notified when {@link com.adjust.sdk.OnAttributionChangedListener}
 * is fired (i.e. this listener would be daisy-chained to the Adjust one). The listener also
 * inherits thread-safety from GeckoSharedPrefs which is used to store the campaign ID.
 */
public interface AttributionHelperListener {
    void onCampaignIdChanged(String campaignId);
}
