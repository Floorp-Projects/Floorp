/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.widget.ArrowPopup;

import org.json.JSONException;
import org.json.JSONObject;

import android.content.res.Resources;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * SiteIdentityPopup is a singleton class that displays site identity data in
 * an arrow panel popup hanging from the lock icon in the browser toolbar.
 */
public class SiteIdentityPopup extends ArrowPopup {
    private static final String LOGTAG = "GeckoSiteIdentityPopup";

    public static final String UNKNOWN = "unknown";
    public static final String VERIFIED = "verified";
    public static final String IDENTIFIED = "identified";

    private Resources mResources;

    private TextView mHost;
    private TextView mOwner;
    private TextView mSupplemental;
    private TextView mVerifier;
    private TextView mEncrypted;
    private ImageView mLarry;

    SiteIdentityPopup(BrowserApp aActivity) {
        super(aActivity, null);

        mResources = aActivity.getResources();
    }

    @Override
    protected void init() {
        super.init();

        // Make the popup focusable so it doesn't inadvertently trigger click events elsewhere
        // which may reshow the popup (see bug 785156)
        setFocusable(true);

        LayoutInflater inflater = LayoutInflater.from(mActivity);
        LinearLayout layout = (LinearLayout) inflater.inflate(R.layout.site_identity, null);
        mContent.addView(layout);

        mHost = (TextView) layout.findViewById(R.id.host);
        mOwner = (TextView) layout.findViewById(R.id.owner);
        mVerifier = (TextView) layout.findViewById(R.id.verifier);
        mLarry = (ImageView) layout.findViewById(R.id.larry);
    }

    /*
     * @param identityData A JSONObject that holds the current tab's identity data.
     */
    public void updateIdentity(JSONObject identityData) {
        String mode;
        try {
            mode = identityData.getString("mode");
        } catch (JSONException e) {
            Log.e(LOGTAG, "Exception trying to get identity mode", e);
            return;
        }

        if (!mode.equals(VERIFIED) && !mode.equals(IDENTIFIED)) {
            Log.e(LOGTAG, "Can't show site identity popup in non-identified state");
            return;
        }

        if (!mInflated)
            init();

        try {
            String host = identityData.getString("host");
            mHost.setText(host);

            String owner = identityData.getString("owner");

            try {
                String supplemental = identityData.getString("supplemental");
                owner += "\n" + supplemental;
            } catch (JSONException e) { }

            mOwner.setText(owner);

            String verifier = identityData.getString("verifier");
            String encrypted = identityData.getString("encrypted");
            mVerifier.setText(verifier + "\n" + encrypted);
        } catch (JSONException e) {
            Log.e(LOGTAG, "Exception trying to get identity data", e);
            return;
        }

        if (mode.equals(VERIFIED)) {
            // Use a blue theme for SSL
            mLarry.setImageResource(R.drawable.larry_blue);
            mHost.setTextColor(mResources.getColor(R.color.identity_verified));
            mOwner.setTextColor(mResources.getColor(R.color.identity_verified));
        } else {
            // Use a green theme for EV
            mLarry.setImageResource(R.drawable.larry_green);
            mHost.setTextColor(mResources.getColor(R.color.identity_identified));
            mOwner.setTextColor(mResources.getColor(R.color.identity_identified));
        }
    }
}
