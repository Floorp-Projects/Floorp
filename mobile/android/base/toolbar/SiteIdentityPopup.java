/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import org.mozilla.gecko.AboutPages;
import org.mozilla.gecko.R;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.SiteIdentity;
import org.mozilla.gecko.SiteIdentity.SecurityMode;
import org.mozilla.gecko.SiteIdentity.MixedMode;
import org.mozilla.gecko.SiteIdentity.TrackingMode;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.widget.ArrowPopup;
import org.mozilla.gecko.widget.DoorHanger;
import org.mozilla.gecko.widget.DoorHanger.OnButtonClickListener;
import org.json.JSONException;
import org.json.JSONObject;

import android.content.Context;
import android.text.TextUtils;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
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

    private static final String TRACKING_CONTENT_SUPPORT_URL =
        "https://support.mozilla.org/kb/how-does-insecure-content-affect-safety-android";

    private SiteIdentity mSiteIdentity;

    private LinearLayout mIdentity;

    private LinearLayout mIdentityKnownContainer;
    private LinearLayout mIdentityUnknownContainer;

    private TextView mHost;
    private TextView mOwner;
    private TextView mVerifier;

    private DoorHanger mMixedContentNotification;
    private DoorHanger mTrackingContentNotification;

    private final OnButtonClickListener mButtonClickListener;

    public SiteIdentityPopup(Context context) {
        super(context);

        mButtonClickListener = new PopupButtonListener();
    }

    @Override
    protected void init() {
        super.init();

        // Make the popup focusable so it doesn't inadvertently trigger click events elsewhere
        // which may reshow the popup (see bug 785156)
        setFocusable(true);

        LayoutInflater inflater = LayoutInflater.from(mContext);
        mIdentity = (LinearLayout) inflater.inflate(R.layout.site_identity, null);
        mContent.addView(mIdentity);

        mIdentityKnownContainer =
                (LinearLayout) mIdentity.findViewById(R.id.site_identity_known_container);
        mIdentityUnknownContainer =
                (LinearLayout) mIdentity.findViewById(R.id.site_identity_unknown_container);

        mHost = (TextView) mIdentityKnownContainer.findViewById(R.id.host);
        mOwner = (TextView) mIdentityKnownContainer.findViewById(R.id.owner);
        mVerifier = (TextView) mIdentityKnownContainer.findViewById(R.id.verifier);
    }

    private void updateIdentity(final SiteIdentity siteIdentity) {
        if (!mInflated) {
            init();
        }

        final MixedMode mixedMode = siteIdentity.getMixedMode();
        final TrackingMode trackingMode = siteIdentity.getTrackingMode();
        if (mixedMode != MixedMode.UNKNOWN || trackingMode != TrackingMode.UNKNOWN) {
            // Hide the identity data if there isn't valid site identity data.
            // Set some top padding on the popup content to create a of light blue
            // between the popup arrow and the mixed content notification.
            mContent.setPadding(0, (int) mContext.getResources().getDimension(R.dimen.identity_padding_top), 0, 0);
            mIdentity.setVisibility(View.GONE);
            return;
        }

        mIdentity.setVisibility(View.VISIBLE);
        mContent.setPadding(0, 0, 0, 0);

        final boolean isIdentityKnown = (siteIdentity.getSecurityMode() != SecurityMode.UNKNOWN);
        toggleIdentityKnownContainerVisibility(isIdentityKnown);

        if (isIdentityKnown) {
            updateIdentityInformation(siteIdentity);
        }
    }

    private void toggleIdentityKnownContainerVisibility(final boolean isIdentityKnown) {
        if (isIdentityKnown) {
            mIdentityKnownContainer.setVisibility(View.VISIBLE);
            mIdentityUnknownContainer.setVisibility(View.GONE);
        } else {
            mIdentityKnownContainer.setVisibility(View.GONE);
            mIdentityUnknownContainer.setVisibility(View.VISIBLE);
        }
    }

    private void updateIdentityInformation(final SiteIdentity siteIdentity) {
        mHost.setText(siteIdentity.getHost());

        String owner = siteIdentity.getOwner();

        // Supplemental data is optional.
        final String supplemental = siteIdentity.getSupplemental();
        if (!TextUtils.isEmpty(supplemental)) {
            owner += "\n" + supplemental;
        }
        mOwner.setText(owner);

        final String verifier = siteIdentity.getVerifier();
        final String encrypted = siteIdentity.getEncrypted();
        mVerifier.setText(verifier + "\n" + encrypted);
    }

    private void addMixedContentNotification(boolean blocked) {
        // Remove any existing mixed content notification.
        removeMixedContentNotification();
        mMixedContentNotification = new DoorHanger(mContext, DoorHanger.Theme.DARK);

        int icon;
        String message;
        if (blocked) {
            icon = R.drawable.shield_doorhanger;
            message = mContext.getString(R.string.blocked_mixed_content_message_top) + "\n\n" +
                      mContext.getString(R.string.blocked_mixed_content_message_bottom);
        } else {
            icon = R.drawable.warning_doorhanger;
            message = mContext.getString(R.string.loaded_mixed_content_message);
        }

        mMixedContentNotification.setIcon(icon);
        mMixedContentNotification.setMessage(message);
        mMixedContentNotification.addLink(mContext.getString(R.string.learn_more), MIXED_CONTENT_SUPPORT_URL, "\n\n");

        addNotificationButtons(mMixedContentNotification, blocked);

        mContent.addView(mMixedContentNotification);
    }

    private void removeMixedContentNotification() {
        if (mMixedContentNotification != null) {
            mContent.removeView(mMixedContentNotification);
            mMixedContentNotification = null;
        }
    }

    private void addTrackingContentNotification(boolean blocked) {
        // Remove any existing tracking content notification.
        removeTrackingContentNotification();
        mTrackingContentNotification = new DoorHanger(mContext, DoorHanger.Theme.DARK);

        int icon;
        String message;
        if (blocked) {
            icon = R.drawable.shield_doorhanger;
            message = mContext.getString(R.string.blocked_tracking_content_message_top) + "\n\n" +
                      mContext.getString(R.string.blocked_tracking_content_message_bottom);
        } else {
            icon = R.drawable.warning_doorhanger;
            message = mContext.getString(R.string.loaded_tracking_content_message);
        }

        mTrackingContentNotification.setIcon(icon);
        mTrackingContentNotification.setMessage(message);
        mTrackingContentNotification.addLink(mContext.getString(R.string.learn_more), TRACKING_CONTENT_SUPPORT_URL, "\n\n");

        addNotificationButtons(mTrackingContentNotification, blocked);

        mContent.addView(mTrackingContentNotification);
    }

    private void removeTrackingContentNotification() {
        if (mTrackingContentNotification != null) {
            mContent.removeView(mTrackingContentNotification);
            mTrackingContentNotification = null;
        }
    }

    private void addNotificationButtons(DoorHanger dh, boolean blocked) {
        if (blocked) {
            dh.addButton(mContext.getString(R.string.disable_protection), "disable", mButtonClickListener);
            dh.addButton(mContext.getString(R.string.keep_blocking), "keepBlocking", mButtonClickListener);
        } else {
            dh.addButton(mContext.getString(R.string.enable_protection), "enable", mButtonClickListener);
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

        // about: has an unknown SiteIdentity in code, but showing "This
        // site's identity is unknown" is misleading! So don't show a popup.
        final Tab selectedTab = Tabs.getInstance().getSelectedTab();
        if (selectedTab != null && AboutPages.isAboutPage(selectedTab.getURL())) {
            Log.d(LOGTAG, "We don't show site identity popups for about: pages");
            return;
        }

        updateIdentity(mSiteIdentity);

        final MixedMode mixedMode = mSiteIdentity.getMixedMode();
        if (mixedMode != MixedMode.UNKNOWN) {
            addMixedContentNotification(mixedMode == MixedMode.MIXED_CONTENT_BLOCKED);
        }

        final TrackingMode trackingMode = mSiteIdentity.getTrackingMode();
        if (trackingMode != TrackingMode.UNKNOWN) {
            addTrackingContentNotification(trackingMode == TrackingMode.TRACKING_CONTENT_BLOCKED);
        }

        showDividers();

        super.show();
    }

    // Show the right dividers
    private void showDividers() {
        final int count = mContent.getChildCount();
        DoorHanger lastVisibleDoorHanger = null;

        for (int i = 0; i < count; i++) {
            final View child = mContent.getChildAt(i);

            if (!(child instanceof DoorHanger)) {
                continue;
            }

            DoorHanger dh = (DoorHanger) child;
            dh.showDivider();
            if (dh.getVisibility() == View.VISIBLE) {
                lastVisibleDoorHanger = dh;
            }
        }

        if (lastVisibleDoorHanger != null) {
            lastVisibleDoorHanger.hideDivider();
        }
    }

    @Override
    public void dismiss() {
        super.dismiss();
        removeMixedContentNotification();
        removeTrackingContentNotification();
    }

    private class PopupButtonListener implements OnButtonClickListener {
        @Override
        public void onButtonClick(DoorHanger dh, String tag) {
            try {
                JSONObject data = new JSONObject();
                String allowType = (dh == mMixedContentNotification ? "allowMixedContent" : "allowTrackingContent");
                data.put(allowType, tag.equals("disable"));

                GeckoEvent e = GeckoEvent.createBroadcastEvent("Session:Reload", data.toString());
                GeckoAppShell.sendEventToGecko(e);
            } catch (JSONException e) {
                Log.e(LOGTAG, "Exception creating message to enable/disable content blocking", e);
            }

            dismiss();
        }
    }
}
