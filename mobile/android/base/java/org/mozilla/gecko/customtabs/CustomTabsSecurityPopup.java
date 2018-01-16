/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.customtabs;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.support.design.widget.Snackbar;
import android.support.v4.content.ContextCompat;
import android.support.v4.widget.TextViewCompat;
import android.widget.ImageView;
import android.widget.Toast;
import org.mozilla.gecko.AboutPages;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoView;
import org.mozilla.gecko.GeckoSession.ProgressListener.SecurityInformation;
import org.mozilla.gecko.R;
import org.mozilla.gecko.SiteIdentity;
import org.mozilla.gecko.SiteIdentity.SecurityMode;
import org.mozilla.gecko.SiteIdentity.MixedMode;
import org.mozilla.gecko.SiteIdentity.TrackingMode;
import org.mozilla.gecko.SnackbarBuilder;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.widget.AnchoredPopup;
import org.mozilla.gecko.widget.DoorHanger;
import org.mozilla.gecko.widget.DoorHanger.OnButtonClickListener;

import android.app.Activity;
import android.content.Context;
import android.text.TextUtils;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;
import org.mozilla.gecko.widget.DoorhangerConfig;
import org.mozilla.gecko.widget.SiteLogins;

/**
 * CustomTabsSecurityPopup is a singleton class that displays site identity data in
 * an arrow panel popup hanging from the lock icon in the browser toolbar.
 *
 * A site identity icon may be displayed in the url, and is set in <code>ToolbarDisplayLayout</code>.
 */
public class CustomTabsSecurityPopup extends AnchoredPopup {
    private static final String LOGTAG = "CustomTabsSecurityPopup";

    private final Resources mResources;
    private SecurityInformation mSecurityInformation;

    private LinearLayout mIdentity;

    private LinearLayout mIdentityKnownContainer;

    private ImageView mIcon;
    private TextView mTitle;
    private TextView mSecurityState;
    private TextView mMixedContentActivity;
    private TextView mOwner;
    private TextView mVerifier;

    public CustomTabsSecurityPopup(Context context) {
        super(context);

        mResources = mContext.getResources();
    }

    @Override
    protected void init() {
        super.init();

        // Make the popup focusable so it doesn't inadvertently trigger click events elsewhere
        // which may reshow the popup (see bug 785156)
        setFocusable(true);

        LayoutInflater inflater = LayoutInflater.from(mContext);
        mIdentity = (LinearLayout) inflater.inflate(R.layout.customtabs_site_identity, null);
        mContent.addView(mIdentity);

        mIdentityKnownContainer =
                (LinearLayout) mIdentity.findViewById(R.id.site_identity_known_container);

        mIcon = (ImageView) mIdentity.findViewById(R.id.site_identity_icon);
        mTitle = (TextView) mIdentity.findViewById(R.id.site_identity_title);
        mSecurityState = (TextView) mIdentity.findViewById(R.id.site_identity_state);
        mMixedContentActivity = (TextView) mIdentity.findViewById(R.id.mixed_content_activity);

        mOwner = (TextView) mIdentityKnownContainer.findViewById(R.id.owner);
        mVerifier = (TextView) mIdentityKnownContainer.findViewById(R.id.verifier);
    }

    private void updateSecurityInformation(final SecurityInformation security) {
        if (!mInflated) {
            init();
        }

        final boolean isIdentityKnown = (SecurityInformation.SECURITY_MODE_IDENTIFIED == security.securityMode ||
                                         SecurityInformation.SECURITY_MODE_VERIFIED == security.securityMode);
        updateConnectionState(security);
        toggleIdentityKnownContainerVisibility(isIdentityKnown);

        if (isIdentityKnown) {
            updateIdentityInformation(security);
        }
    }

    private void toggleIdentityKnownContainerVisibility(final boolean isIdentityKnown) {
        final int identityInfoVisibility = isIdentityKnown ? View.VISIBLE : View.GONE;
        mIdentityKnownContainer.setVisibility(identityInfoVisibility);
    }

    /**
     * Update the SecurityInformation content to reflect connection state.
     *
     * The connection state should reflect the combination of:
     * a) Connection encryption
     * b) Mixed Content state (Active/Display Mixed content, loaded, blocked, none, etc)
     * and update the icons and strings to inform the user of that state.
     *
     * @param security SecurityInformation about the connection.
     */
    private void updateConnectionState(final SecurityInformation security) {
        if (!security.isSecure) {
            if (SecurityInformation.CONTENT_LOADED == security.mixedModeActive) {
                // Active Mixed Content loaded because user has disabled blocking.
                mIcon.setImageResource(R.drawable.ic_lock_disabled);
                clearSecurityStateIcon();
                mMixedContentActivity.setVisibility(View.VISIBLE);
                mMixedContentActivity.setText(R.string.mixed_content_protection_disabled);
            } else if (SecurityInformation.CONTENT_LOADED == security.mixedModePassive) {
                // Passive Mixed Content loaded.
                mIcon.setImageResource(R.drawable.ic_lock_inactive);
                setSecurityStateIcon(R.drawable.ic_warning_major, 1);
                mMixedContentActivity.setVisibility(View.VISIBLE);
                if (SecurityInformation.CONTENT_BLOCKED == security.mixedModeActive) {
                    mMixedContentActivity.setText(R.string.mixed_content_blocked_some);
                } else {
                    mMixedContentActivity.setText(R.string.mixed_content_display_loaded);
                }
            } else {
                // Unencrypted connection with no mixed content.
                mIcon.setImageResource(R.drawable.globe_light);
                clearSecurityStateIcon();

                mMixedContentActivity.setVisibility(View.GONE);
            }

            mSecurityState.setText(R.string.identity_connection_insecure);
            mSecurityState.setTextColor(ContextCompat.getColor(mContext, R.color.placeholder_active_grey));

        } else if (security.isException) {
            mIcon.setImageResource(R.drawable.ic_lock_inactive);
            setSecurityStateIcon(R.drawable.ic_warning_major, 1);
            mSecurityState.setText(R.string.identity_connection_insecure);
            mSecurityState.setTextColor(ContextCompat.getColor(mContext, R.color.placeholder_active_grey));

        } else {
            // Connection is secure.
            mIcon.setImageResource(R.drawable.ic_lock);

            setSecurityStateIcon(R.drawable.img_check, 2);
            mSecurityState.setTextColor(ContextCompat.getColor(mContext, R.color.affirmative_green));
            mSecurityState.setText(R.string.identity_connection_secure);

            // Mixed content has been blocked, if present.
            if (SecurityInformation.CONTENT_BLOCKED == security.mixedModeActive ||
                SecurityInformation.CONTENT_BLOCKED == security.mixedModePassive) {
                mMixedContentActivity.setVisibility(View.VISIBLE);
                mMixedContentActivity.setText(R.string.mixed_content_blocked_all);
            } else {
                mMixedContentActivity.setVisibility(View.GONE);
            }
        }
    }

    private void clearSecurityStateIcon() {
        mSecurityState.setCompoundDrawablePadding(0);
        TextViewCompat.setCompoundDrawablesRelative(mSecurityState, null, null, null, null);
    }

    private void setSecurityStateIcon(int resource, int factor) {
        final Drawable stateIcon = ContextCompat.getDrawable(mContext, resource);
        stateIcon.setBounds(0, 0, stateIcon.getIntrinsicWidth() / factor, stateIcon.getIntrinsicHeight() / factor);
        TextViewCompat.setCompoundDrawablesRelative(mSecurityState, stateIcon, null, null, null);
        mSecurityState.setCompoundDrawablePadding((int) mResources.getDimension(R.dimen.doorhanger_drawable_padding));
    }

    private void updateIdentityInformation(final SecurityInformation security) {
        String owner = security.organization;
        if (owner == null) {
            mOwner.setVisibility(View.GONE);
        } else {
            mOwner.setText(owner);
            mOwner.setVisibility(View.VISIBLE);
        }

        // TODO: This will differ from Fennec currently, as SiteIdentityPopup uses a localized string
        mVerifier.setText(security.issuerCommonName);
    }

    /*
     * @param identityData An object that holds the current tab's identity data.
     */
    public void setSecurityInformation(SecurityInformation security) {
        mSecurityInformation = security;
    }

    @Override
    public void show() {
        if (mSecurityInformation == null) {
            Log.e(LOGTAG, "Can't show site identity popup for undefined state");
            return;
        }

        updateSecurityInformation(mSecurityInformation);

        mTitle.setText(mSecurityInformation.host);

        // TODO: Might be nice to revive this at some point, but very low priority.
        /*final Bitmap favicon = selectedTab.getFavicon();
        if (favicon != null) {
            final Drawable faviconDrawable = new BitmapDrawable(mResources, favicon);
            final int dimen = (int) mResources.getDimension(R.dimen.browser_toolbar_favicon_size);
            faviconDrawable.setBounds(0, 0, dimen, dimen);

            TextViewCompat.setCompoundDrawablesRelative(mTitle, faviconDrawable, null, null, null);
            mTitle.setCompoundDrawablePadding((int) mContext.getResources().getDimension(R.dimen.doorhanger_drawable_padding));
        }*/

        super.show();
    }

    @Override
    public void dismiss() {
        super.dismiss();
        TextViewCompat.setCompoundDrawablesRelativeWithIntrinsicBounds(mTitle, null, null, null, null);
    }
}
