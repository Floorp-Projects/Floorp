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
public class SiteIdentityPopup extends ArrowPopup
                               implements DoorHanger.OnButtonClickListener {
    private static final String LOGTAG = "GeckoSiteIdentityPopup";

    public static final String UNKNOWN = "unknown";
    public static final String VERIFIED = "verified";
    public static final String IDENTIFIED = "identified";
    public static final String MIXED_CONTENT_BLOCKED = "mixed_content_blocked";
    public static final String MIXED_CONTENT_LOADED = "mixed_content_loaded";

    // Security states corresponding to image levels in site_security_level.xml
    public static final int LEVEL_UKNOWN = 0;
    public static final int LEVEL_IDENTIFIED = 1;
    public static final int LEVEL_VERIFIED = 2;
    public static final int LEVEL_MIXED_CONTENT_BLOCKED = 3;
    public static final int LEVEL_MIXED_CONTENT_LOADED = 4;

    // FIXME: Update this URL for mobile. See bug 885923.
    private static final String MIXED_CONTENT_SUPPORT_URL =
        "https://support.mozilla.org/kb/how-does-content-isnt-secure-affect-my-safety";

    private Resources mResources;

    private TextView mHost;
    private TextView mOwner;
    private TextView mSupplemental;
    private TextView mVerifier;
    private TextView mEncrypted;
    private ImageView mLarry;

    private DoorHanger mMixedContentNotification;

    SiteIdentityPopup(BrowserApp aActivity) {
        super(aActivity, null);

        mResources = aActivity.getResources();
    }

    public static int getSecurityImageLevel(String mode) {
        if (IDENTIFIED.equals(mode)) {
            return LEVEL_IDENTIFIED;
        }
        if (VERIFIED.equals(mode)) {
            return LEVEL_VERIFIED;
        }
        if (MIXED_CONTENT_BLOCKED.equals(mode)) {
            return LEVEL_MIXED_CONTENT_BLOCKED;
        }
        if (MIXED_CONTENT_LOADED.equals(mode)) {
            return LEVEL_MIXED_CONTENT_LOADED;
        }
        return LEVEL_UKNOWN;
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

    private void setIdentity(JSONObject identityData) {
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
        }
    }

    @Override
    public void onButtonClick(DoorHanger dh, String tag) {
        if (tag.equals("disable")) {
            // To disable mixed content blocking, reload the page with a flag to load mixed content.
            try {
                JSONObject data = new JSONObject();
                data.put("allowMixedContent", true);
                GeckoEvent e = GeckoEvent.createBroadcastEvent("Session:Reload", data.toString());
                GeckoAppShell.sendEventToGecko(e);
            } catch (JSONException e) {
                Log.e(LOGTAG, "Exception creating message to allow mixed content", e);
            }
        } else if (tag.equals("enable")) {
            // To enable mixed content blocking, reload the page without any flags.
            GeckoEvent e = GeckoEvent.createBroadcastEvent("Session:Reload", "");
            GeckoAppShell.sendEventToGecko(e);
        }

        dismiss();
    }

    private void addMixedContentNotification(boolean blocked) {
        // Remove any exixting mixed content notification.
        removeMixedContentNotification();
        mMixedContentNotification = new DoorHanger(mActivity);

        String message;
        if (blocked) {
            message = mActivity.getString(R.string.blocked_mixed_content_message_top) + "\n\n" +
                      mActivity.getString(R.string.blocked_mixed_content_message_bottom);
        } else {
            message = mActivity.getString(R.string.loaded_mixed_content_message);
        }
        mMixedContentNotification.setMessage(message);
        mMixedContentNotification.addLink(mActivity.getString(R.string.learn_more), MIXED_CONTENT_SUPPORT_URL, "\n\n");

        if (blocked) {
            mMixedContentNotification.addButton(mActivity.getString(R.string.disable_protection), "disable", this);
            mMixedContentNotification.addButton(mActivity.getString(R.string.keep_blocking), "keepBlocking", this);
        } else {
            mMixedContentNotification.addButton(mActivity.getString(R.string.enable_protection), "enable", this);
        }
        mMixedContentNotification.hideDivider();
        mMixedContentNotification.setBackgroundColor(0xFFDDE4EA);

        mContent.addView(mMixedContentNotification);
    }

    private void removeMixedContentNotification() {
        if (mMixedContentNotification != null) {
            mContent.removeView(mMixedContentNotification);
            mMixedContentNotification = null;
        }
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

        if (UNKNOWN.equals(mode)) {
            Log.e(LOGTAG, "Can't show site identity popup in non-identified state");
            return;
        }

        if (!mInflated)
            init();

        setIdentity(identityData);

        if (VERIFIED.equals(mode)) {
            // Use a blue theme for SSL
            mLarry.setImageResource(R.drawable.larry_blue);
            mHost.setTextColor(mResources.getColor(R.color.identity_verified));
            mOwner.setTextColor(mResources.getColor(R.color.identity_verified));
        } else if (IDENTIFIED.equals(mode)) {
            // Use a green theme for EV
            mLarry.setImageResource(R.drawable.larry_green);
            mHost.setTextColor(mResources.getColor(R.color.identity_identified));
            mOwner.setTextColor(mResources.getColor(R.color.identity_identified));
        } else {
            // Use a gray theme for sites with mixed content
            // FIXME: Get a gray larry
            mLarry.setImageResource(R.drawable.larry_blue);
            mHost.setTextColor(mResources.getColor(R.color.identity_mixed_content));
            mOwner.setTextColor(mResources.getColor(R.color.identity_mixed_content));

            addMixedContentNotification(MIXED_CONTENT_BLOCKED.equals(mode));
        }
    }

    @Override
    public void dismiss() {
        super.dismiss();
        removeMixedContentNotification();
    }
}
