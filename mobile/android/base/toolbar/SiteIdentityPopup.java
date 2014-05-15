/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.R;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.SiteIdentity;
import org.mozilla.gecko.SiteIdentity.SecurityMode;
import org.mozilla.gecko.widget.ArrowPopup;
import org.mozilla.gecko.widget.DoorHanger;
import org.mozilla.gecko.widget.DoorHanger.OnButtonClickListener;

import org.json.JSONException;
import org.json.JSONObject;

import android.content.res.Resources;
import android.text.TextUtils;
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

    private static final String MIXED_CONTENT_SUPPORT_URL =
        "https://support.mozilla.org/kb/how-does-insecure-content-affect-safety-android";

    private SiteIdentity mSiteIdentity;

    private Resources mResources;

    private LinearLayout mIdentity;
    private TextView mHost;
    private TextView mOwner;
    private TextView mVerifier;

    private DoorHanger mMixedContentNotification;

    private final OnButtonClickListener mButtonClickListener;

    SiteIdentityPopup(BrowserApp activity) {
        super(activity);

        mResources = activity.getResources();
        mButtonClickListener = new PopupButtonListener();
    }

    @Override
    protected void init() {
        super.init();

        // Make the popup focusable so it doesn't inadvertently trigger click events elsewhere
        // which may reshow the popup (see bug 785156)
        setFocusable(true);

        LayoutInflater inflater = LayoutInflater.from(mActivity);
        mIdentity = (LinearLayout) inflater.inflate(R.layout.site_identity, null);
        mContent.addView(mIdentity);

        mHost = (TextView) mIdentity.findViewById(R.id.host);
        mOwner = (TextView) mIdentity.findViewById(R.id.owner);
        mVerifier = (TextView) mIdentity.findViewById(R.id.verifier);
    }

    private void updateUi() {
        if (!mInflated) {
            init();
        }

        if (mSiteIdentity.getSecurityMode() == SecurityMode.MIXED_CONTENT_LOADED ||
            mSiteIdentity.getSecurityMode() == SecurityMode.MIXED_CONTENT_BLOCKED) {
            // Hide the identity data if there isn't valid site identity data.
            // Set some top padding on the popup content to create a of light blue
            // between the popup arrow and the mixed content notification.
            mContent.setPadding(0, (int) mResources.getDimension(R.dimen.identity_padding_top), 0, 0);
            mIdentity.setVisibility(View.GONE);
        } else {
            mHost.setText(mSiteIdentity.getHost());

            String owner = mSiteIdentity.getOwner();

            // Supplemental data is optional.
            final String supplemental = mSiteIdentity.getSupplemental();
            if (!TextUtils.isEmpty(supplemental)) {
                owner += "\n" + supplemental;
            }
            mOwner.setText(owner);

            final String verifier = mSiteIdentity.getVerifier();
            final String encrypted = mSiteIdentity.getEncrypted();
            mVerifier.setText(verifier + "\n" + encrypted);

            mContent.setPadding(0, 0, 0, 0);
            mIdentity.setVisibility(View.VISIBLE);
        }
    }

    private void addMixedContentNotification(boolean blocked) {
        // Remove any exixting mixed content notification.
        removeMixedContentNotification();
        mMixedContentNotification = new DoorHanger(mActivity, DoorHanger.Theme.DARK);

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
            mMixedContentNotification.setIcon(R.drawable.shield_doorhanger);
            mMixedContentNotification.addButton(mActivity.getString(R.string.disable_protection),
                                                "disable", mButtonClickListener);
            mMixedContentNotification.addButton(mActivity.getString(R.string.keep_blocking),
                                                "keepBlocking", mButtonClickListener);
        } else {
            mMixedContentNotification.setIcon(R.drawable.warning_doorhanger);
            mMixedContentNotification.addButton(mActivity.getString(R.string.enable_protection),
                                                "enable", mButtonClickListener);
        }

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
    void setSiteIdentity(SiteIdentity siteIdentity) {
        mSiteIdentity = siteIdentity;
    }

    @Override
    public void show() {
        if (mSiteIdentity == null) {
            Log.e(LOGTAG, "Can't show site identity popup for undefined state");
            return;
        }

        final SecurityMode mode = mSiteIdentity.getSecurityMode();
        if (mode == SecurityMode.UNKNOWN) {
            Log.e(LOGTAG, "Can't show site identity popup in non-identified state");
            return;
        }

        updateUi();

        if (mode == SecurityMode.MIXED_CONTENT_LOADED ||
            mode == SecurityMode.MIXED_CONTENT_BLOCKED) {
            addMixedContentNotification(mode == SecurityMode.MIXED_CONTENT_BLOCKED);
        }

        super.show();
    }

    @Override
    public void dismiss() {
        super.dismiss();
        removeMixedContentNotification();
    }

    private class PopupButtonListener implements OnButtonClickListener {
        @Override
        public void onButtonClick(DoorHanger dh, String tag) {
            try {
                JSONObject data = new JSONObject();
                data.put("allowMixedContent", tag.equals("disable"));
                GeckoEvent e = GeckoEvent.createBroadcastEvent("Session:Reload", data.toString());
                GeckoAppShell.sendEventToGecko(e);
            } catch (JSONException e) {
                Log.e(LOGTAG, "Exception creating message to enable/disable mixed content blocking", e);
            }

            dismiss();
        }
    }
}
