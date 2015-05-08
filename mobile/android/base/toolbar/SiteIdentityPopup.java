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
import org.mozilla.gecko.widget.AnchoredPopup;
import org.mozilla.gecko.widget.DoorHanger;
import org.mozilla.gecko.widget.DoorHanger.OnButtonClickListener;
import org.json.JSONObject;

import android.content.Context;
import android.text.TextUtils;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;
import org.mozilla.gecko.widget.DoorhangerConfig;

/**
 * SiteIdentityPopup is a singleton class that displays site identity data in
 * an arrow panel popup hanging from the lock icon in the browser toolbar.
 */
public class SiteIdentityPopup extends AnchoredPopup {
    public static enum ButtonType { DISABLE, ENABLE, KEEP_BLOCKING };

    private static final String LOGTAG = "GeckoSiteIdentityPopup";

    private static final String MIXED_CONTENT_SUPPORT_URL =
        "https://support.mozilla.org/kb/how-does-insecure-content-affect-safety-android";

    private static final String TRACKING_CONTENT_SUPPORT_URL =
        "https://support.mozilla.org/kb/firefox-android-tracking-protection";

    private SiteIdentity mSiteIdentity;

    private LinearLayout mIdentity;

    private LinearLayout mIdentityKnownContainer;
    private LinearLayout mIdentityUnknownContainer;

    private TextView mHost;
    private TextView mOwnerLabel;
    private TextView mOwner;
    private TextView mVerifier;

    private View mDivider;

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
        mOwnerLabel = (TextView) mIdentityKnownContainer.findViewById(R.id.owner_label);
        mOwner = (TextView) mIdentityKnownContainer.findViewById(R.id.owner);
        mVerifier = (TextView) mIdentityKnownContainer.findViewById(R.id.verifier);
        mDivider = mIdentity.findViewById(R.id.divider_doorhanger);

        final TextView siteSettingsLink = (TextView) mIdentity.findViewById(R.id.site_settings_link);
        siteSettingsLink.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Permissions:Get", null));
                dismiss();
            }
        });
    }

    private void updateIdentity(final SiteIdentity siteIdentity) {
        if (!mInflated) {
            init();
        }

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
        if (owner == null) {
            mOwnerLabel.setVisibility(View.GONE);
            mOwner.setVisibility(View.GONE);
        } else {
            mOwnerLabel.setVisibility(View.VISIBLE);
            mOwner.setVisibility(View.VISIBLE);

            // Supplemental data is optional.
            final String supplemental = siteIdentity.getSupplemental();
            if (!TextUtils.isEmpty(supplemental)) {
                owner += "\n" + supplemental;
            }
            mOwner.setText(owner);
        }

        final String verifier = siteIdentity.getVerifier();
        final String encrypted = siteIdentity.getEncrypted();
        mVerifier.setText(verifier + "\n" + encrypted);
    }

    private void addMixedContentNotification(boolean blocked) {
        // Remove any existing mixed content notification.
        removeMixedContentNotification();

        final DoorhangerConfig config = new DoorhangerConfig(DoorHanger.Type.MIXED_CONTENT, mButtonClickListener);
        int icon;
        if (blocked) {
            icon = R.drawable.shield_enabled_doorhanger;
            config.setMessage(mContext.getString(R.string.blocked_mixed_content_message_top) + "\n\n" +
                      mContext.getString(R.string.blocked_mixed_content_message_bottom));
        } else {
            icon = R.drawable.shield_disabled_doorhanger;
            config.setMessage(mContext.getString(R.string.loaded_mixed_content_message));
        }

        config.setLink(mContext.getString(R.string.learn_more), MIXED_CONTENT_SUPPORT_URL, "\n\n");
        addNotificationButtons(config, blocked);

        mMixedContentNotification = DoorHanger.Get(mContext, config);
        mMixedContentNotification.setIcon(icon);


        mContent.addView(mMixedContentNotification);
        mDivider.setVisibility(View.VISIBLE);
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

        final DoorhangerConfig config = new DoorhangerConfig(DoorHanger.Type.TRACKING, mButtonClickListener);

        int icon;
        if (blocked) {
            icon = R.drawable.shield_enabled_doorhanger;
            config.setMessage(mContext.getString(R.string.blocked_tracking_content_message_top) + "\n\n" +
                      mContext.getString(R.string.blocked_tracking_content_message_bottom));
        } else {
            icon = R.drawable.shield_disabled_doorhanger;
            config.setMessage(mContext.getString(R.string.loaded_tracking_content_message_top) + "\n\n" +
                      mContext.getString(R.string.loaded_tracking_content_message_bottom));
        }

        config.setLink(mContext.getString(R.string.learn_more), TRACKING_CONTENT_SUPPORT_URL, "\n\n");
        addNotificationButtons(config, blocked);

        mTrackingContentNotification = DoorHanger.Get(mContext, config);

        mTrackingContentNotification.setIcon(icon);


        mContent.addView(mTrackingContentNotification);
        mDivider.setVisibility(View.VISIBLE);
    }

    private void removeTrackingContentNotification() {
        if (mTrackingContentNotification != null) {
            mContent.removeView(mTrackingContentNotification);
            mTrackingContentNotification = null;
        }
    }

    private void addNotificationButtons(DoorhangerConfig config, boolean blocked) {
        if (blocked) {
            config.appendButton(mContext.getString(R.string.disable_protection), ButtonType.DISABLE.ordinal());
            config.appendButton(mContext.getString(R.string.keep_blocking), ButtonType.KEEP_BLOCKING.ordinal());
        } else {
            config.appendButton(mContext.getString(R.string.enable_protection), ButtonType.ENABLE.ordinal());
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
        mDivider.setVisibility(View.GONE);
    }

    private class PopupButtonListener implements OnButtonClickListener {
        @Override
        public void onButtonClick(JSONObject response, DoorHanger doorhanger) {
            GeckoEvent e = GeckoEvent.createBroadcastEvent("Session:Reload", response.toString());
            GeckoAppShell.sendEventToGecko(e);
            dismiss();
        }
    }
}
