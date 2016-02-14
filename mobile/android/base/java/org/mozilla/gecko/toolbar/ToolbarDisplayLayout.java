/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import java.util.Arrays;
import java.util.EnumSet;
import java.util.List;

import org.mozilla.gecko.AboutPages;
import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.R;
import org.mozilla.gecko.ReaderModeUtils;
import org.mozilla.gecko.SiteIdentity;
import org.mozilla.gecko.SiteIdentity.MixedMode;
import org.mozilla.gecko.SiteIdentity.SecurityMode;
import org.mozilla.gecko.SiteIdentity.TrackingMode;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.animation.ViewHelper;
import org.mozilla.gecko.toolbar.BrowserToolbarTabletBase.ForwardButtonAnimation;
import org.mozilla.gecko.util.ColorUtils;
import org.mozilla.gecko.util.StringUtils;
import org.mozilla.gecko.widget.themed.ThemedLinearLayout;
import org.mozilla.gecko.widget.themed.ThemedTextView;

import android.content.Context;
import android.content.res.Resources;
import android.os.SystemClock;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.TextUtils;
import android.text.style.ForegroundColorSpan;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.animation.AlphaAnimation;
import android.view.animation.Animation;
import android.view.animation.TranslateAnimation;
import android.widget.Button;
import android.widget.ImageButton;

/**
* {@code ToolbarDisplayLayout} is the UI for when the toolbar is in
* display state. It's used to display the state of the currently selected
* tab. It should always be updated through a single entry point
* (updateFromTab) and should never track any tab events or gecko messages
* on its own to keep it as dumb as possible.
*
* The UI has two possible modes: progress and display which are triggered
* when UpdateFlags.PROGRESS is used depending on the current tab state.
* The progress mode is triggered when the tab is loading a page. Display mode
* is used otherwise.
*
* {@code ToolbarDisplayLayout} is meant to be owned by {@code BrowserToolbar}
* which is the main event bus for the toolbar subsystem.
*/
public class ToolbarDisplayLayout extends ThemedLinearLayout {

    private static final String LOGTAG = "GeckoToolbarDisplayLayout";
    private boolean mTrackingProtectionEnabled;

    // To be used with updateFromTab() to allow the caller
    // to give enough context for the requested state change.
    enum UpdateFlags {
        TITLE,
        FAVICON,
        PROGRESS,
        SITE_IDENTITY,
        PRIVATE_MODE,

        // Disable any animation that might be
        // triggered from this state change. Mostly
        // used on tab switches, see BrowserToolbar.
        DISABLE_ANIMATIONS
    }

    private enum UIMode {
        PROGRESS,
        DISPLAY
    }

    interface OnStopListener {
        public Tab onStop();
    }

    interface OnTitleChangeListener {
        public void onTitleChange(CharSequence title);
    }

    private final BrowserApp mActivity;

    private UIMode mUiMode;

    private boolean mIsAttached;

    private final ThemedTextView mTitle;
    private final int mTitlePadding;
    private ToolbarPrefs mPrefs;
    private OnTitleChangeListener mTitleChangeListener;

    private final ImageButton mSiteSecurity;

    private final ImageButton mStop;
    private OnStopListener mStopListener;

    private final PageActionLayout mPageActionLayout;

    private final SiteIdentityPopup mSiteIdentityPopup;
    private int mSecurityImageLevel;

    // Security level constants, which map to the icons / levels defined in:
    // http://mxr.mozilla.org/mozilla-central/source/mobile/android/base/java/org/mozilla/gecko/resources/drawable/site_security_level.xml
    // Default level (unverified pages) - globe icon:
    private final int LEVEL_DEFAULT_GLOBE = 0;
    // Levels for displaying Mixed Content state icons.
    private final int LEVEL_WARNING_MINOR = 3;
    private final int LEVEL_LOCK_DISABLED = 4;
    // Levels for displaying Tracking Protection state icons.
    private final int LEVEL_SHIELD_ENABLED = 5;
    private final int LEVEL_SHIELD_DISABLED = 6;

    private final ForegroundColorSpan mUrlColor;
    private final ForegroundColorSpan mBlockedColor;
    private final ForegroundColorSpan mDomainColor;
    private final ForegroundColorSpan mPrivateDomainColor;

    public ToolbarDisplayLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        setOrientation(HORIZONTAL);

        mActivity = (BrowserApp) context;

        LayoutInflater.from(context).inflate(R.layout.toolbar_display_layout, this);

        mTitle = (ThemedTextView) findViewById(R.id.url_bar_title);
        mTitlePadding = mTitle.getPaddingRight();

        final Resources res = getResources();

        mUrlColor = new ForegroundColorSpan(ColorUtils.getColor(context, R.color.url_bar_urltext));
        mBlockedColor = new ForegroundColorSpan(ColorUtils.getColor(context, R.color.url_bar_blockedtext));
        mDomainColor = new ForegroundColorSpan(ColorUtils.getColor(context, R.color.url_bar_domaintext));
        mPrivateDomainColor = new ForegroundColorSpan(ColorUtils.getColor(context, R.color.url_bar_domaintext_private));

        mSiteSecurity = (ImageButton) findViewById(R.id.site_security);

        mSiteIdentityPopup = new SiteIdentityPopup(mActivity);
        mSiteIdentityPopup.setAnchor(this);
        mSiteIdentityPopup.setOnVisibilityChangeListener(mActivity);

        mStop = (ImageButton) findViewById(R.id.stop);
        mPageActionLayout = (PageActionLayout) findViewById(R.id.page_action_layout);
    }

    @Override
    public void onAttachedToWindow() {
        mIsAttached = true;

        mSiteSecurity.setOnClickListener(new Button.OnClickListener() {
            @Override
            public void onClick(View view) {
                mSiteIdentityPopup.show();
            }
        });

        mStop.setOnClickListener(new Button.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mStopListener != null) {
                    // Force toolbar to switch to Display mode
                    // immediately based on the stopped tab.
                    final Tab tab = mStopListener.onStop();
                    if (tab != null) {
                        updateUiMode(UIMode.DISPLAY);
                    }
                }
            }
        });
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        mIsAttached = false;
    }

    @Override
    public void setNextFocusDownId(int nextId) {
        mStop.setNextFocusDownId(nextId);
        mSiteSecurity.setNextFocusDownId(nextId);
        mPageActionLayout.setNextFocusDownId(nextId);
    }

    void setToolbarPrefs(final ToolbarPrefs prefs) {
        mPrefs = prefs;
    }

    void updateFromTab(Tab tab, EnumSet<UpdateFlags> flags) {
        // Several parts of ToolbarDisplayLayout's state depends
        // on the views being attached to the view tree.
        if (!mIsAttached) {
            return;
        }

        if (flags.contains(UpdateFlags.TITLE)) {
            updateTitle(tab);
        }

        if (flags.contains(UpdateFlags.SITE_IDENTITY)) {
            updateSiteIdentity(tab);
        }

        if (flags.contains(UpdateFlags.PROGRESS)) {
            updateProgress(tab);
        }

        if (flags.contains(UpdateFlags.PRIVATE_MODE)) {
            mTitle.setPrivateMode(tab != null && tab.isPrivate());
        }
    }

    void setTitle(CharSequence title) {
        mTitle.setText(title);

        if (mTitleChangeListener != null) {
            mTitleChangeListener.onTitleChange(title);
        }
    }

    private void updateTitle(Tab tab) {
        // Keep the title unchanged if there's no selected tab,
        // or if the tab is entering reader mode.
        if (tab == null || tab.isEnteringReaderMode()) {
            return;
        }

        final String url = tab.getURL();

        // Setting a null title will ensure we just see the
        // "Enter Search or Address" placeholder text.
        if (AboutPages.isTitlelessAboutPage(url)) {
            setTitle(null);
            return;
        }

        // Show the about:blocked page title in red, regardless of prefs
        if (tab.getErrorType() == Tab.ErrorType.BLOCKED) {
            final String title = tab.getDisplayTitle();

            final SpannableStringBuilder builder = new SpannableStringBuilder(title);
            builder.setSpan(mBlockedColor, 0, title.length(), Spannable.SPAN_INCLUSIVE_INCLUSIVE);

            setTitle(builder);
            return;
        }

        String strippedURL = stripAboutReaderURL(url);

        if (mPrefs.shouldTrimUrls()) {
            strippedURL = StringUtils.stripCommonSubdomains(StringUtils.stripScheme(strippedURL));
        }

        CharSequence title = strippedURL;

        final String baseDomain = tab.getBaseDomain();
        if (!TextUtils.isEmpty(baseDomain)) {
            final SpannableStringBuilder builder = new SpannableStringBuilder(title);

            int index = title.toString().indexOf(baseDomain);
            if (index > -1) {
                builder.setSpan(mUrlColor, 0, title.length(), Spannable.SPAN_INCLUSIVE_INCLUSIVE);
                builder.setSpan(tab.isPrivate() ? mPrivateDomainColor : mDomainColor,
                                index, index + baseDomain.length(), Spannable.SPAN_INCLUSIVE_INCLUSIVE);

                title = builder;
            }
        }

        setTitle(title);
    }

    private String stripAboutReaderURL(final String url) {
        if (!AboutPages.isAboutReader(url)) {
            return url;
        }

        return ReaderModeUtils.getUrlFromAboutReader(url);
    }

    private void updateSiteIdentity(Tab tab) {
        final SiteIdentity siteIdentity;
        if (tab == null) {
            siteIdentity = null;
        } else {
            siteIdentity = tab.getSiteIdentity();
        }

        mSiteIdentityPopup.setSiteIdentity(siteIdentity);

        final SecurityMode securityMode;
        final MixedMode activeMixedMode;
        final MixedMode displayMixedMode;
        final TrackingMode trackingMode;
        final boolean loginInsecure;
        if (siteIdentity == null) {
            securityMode = SecurityMode.UNKNOWN;
            activeMixedMode = MixedMode.UNKNOWN;
            displayMixedMode = MixedMode.UNKNOWN;
            trackingMode = TrackingMode.UNKNOWN;
            loginInsecure = false;
        } else {
            securityMode = siteIdentity.getSecurityMode();
            activeMixedMode = siteIdentity.getMixedModeActive();
            displayMixedMode = siteIdentity.getMixedModeDisplay();
            trackingMode = siteIdentity.getTrackingMode();
            loginInsecure = siteIdentity.loginInsecure();
        }

        // This is a bit tricky, but we have one icon and three potential indicators.
        // Default to the identity level
        int imageLevel = securityMode.ordinal();

        // about: pages should default to having no icon too (the same as SecurityMode.UNKNOWN), however
        // SecurityMode.CHROMEUI has a different ordinal - hence we need to manually reset it here.
        // (We then continue and process the tracking / mixed content icons as usual, even for about: pages, as they
        //  can still load external sites.)
        if (securityMode == SecurityMode.CHROMEUI) {
            imageLevel = LEVEL_DEFAULT_GLOBE; // == SecurityMode.UNKNOWN.ordinal()
        }

        // Check to see if any protection was overridden first
        if (loginInsecure) {
            imageLevel = LEVEL_LOCK_DISABLED;
        } else if (trackingMode == TrackingMode.TRACKING_CONTENT_LOADED) {
            imageLevel = LEVEL_SHIELD_DISABLED;
        } else if (trackingMode == TrackingMode.TRACKING_CONTENT_BLOCKED) {
            imageLevel = LEVEL_SHIELD_ENABLED;
        } else if (activeMixedMode == MixedMode.MIXED_CONTENT_LOADED) {
            imageLevel = LEVEL_LOCK_DISABLED;
        } else if (displayMixedMode == MixedMode.MIXED_CONTENT_LOADED) {
            imageLevel = LEVEL_WARNING_MINOR;
        }

        if (mSecurityImageLevel != imageLevel) {
            mSecurityImageLevel = imageLevel;
            mSiteSecurity.setImageLevel(mSecurityImageLevel);
            updatePageActions();
        }

        mTrackingProtectionEnabled = trackingMode == TrackingMode.TRACKING_CONTENT_BLOCKED;
    }

    private void updateProgress(Tab tab) {
        final boolean shouldShowThrobber = (tab != null &&
                                            tab.getState() == Tab.STATE_LOADING);

        updateUiMode(shouldShowThrobber ? UIMode.PROGRESS : UIMode.DISPLAY);

        if (Tab.STATE_SUCCESS == tab.getState() && mTrackingProtectionEnabled) {
            mActivity.showTrackingProtectionPromptIfApplicable();
        }
    }

    private void updateUiMode(UIMode uiMode) {
        if (mUiMode == uiMode) {
            return;
        }

        mUiMode = uiMode;

        // The "Throbber start" and "Throbber stop" log messages in this method
        // are needed by S1/S2 tests (http://mrcote.info/phonedash/#).
        // See discussion in Bug 804457. Bug 805124 tracks paring these down.
        if (mUiMode == UIMode.PROGRESS) {
            Log.i(LOGTAG, "zerdatime " + SystemClock.uptimeMillis() + " - Throbber start");
        } else {
            Log.i(LOGTAG, "zerdatime " + SystemClock.uptimeMillis() + " - Throbber stop");
        }

        updatePageActions();
    }

    private void updatePageActions() {
        final boolean isShowingProgress = (mUiMode == UIMode.PROGRESS);

        mStop.setVisibility(isShowingProgress ? View.VISIBLE : View.GONE);
        mPageActionLayout.setVisibility(!isShowingProgress ? View.VISIBLE : View.GONE);

        // We want title to fill the whole space available for it when there are icons
        // being shown on the right side of the toolbar as the icons already have some
        // padding in them. This is just to avoid wasting space when icons are shown.
        mTitle.setPadding(0, 0, (!isShowingProgress ? mTitlePadding : 0), 0);
    }

    List<View> getFocusOrder() {
        return Arrays.asList(mSiteSecurity, mPageActionLayout, mStop);
    }

    void setOnStopListener(OnStopListener listener) {
        mStopListener = listener;
    }

    void setOnTitleChangeListener(OnTitleChangeListener listener) {
        mTitleChangeListener = listener;
    }

    /**
     * Update the Site Identity popup anchor.
     *
     * Tablet UI has a tablet-specific doorhanger anchor, so update it after all the views
     * are inflated.
     * @param view View to use as the anchor for the Site Identity popup.
     */
    void updateSiteIdentityAnchor(View view) {
        mSiteIdentityPopup.setAnchor(view);
    }

    void prepareForwardAnimation(PropertyAnimator anim, ForwardButtonAnimation animation, int width) {
        if (animation == ForwardButtonAnimation.HIDE) {
            // We animate these items individually, rather than this entire view,
            // so that we don't animate certain views, e.g. the stop button.
            anim.attach(mTitle,
                        PropertyAnimator.Property.TRANSLATION_X,
                        0);
            anim.attach(mSiteSecurity,
                        PropertyAnimator.Property.TRANSLATION_X,
                        0);

            // We're hiding the forward button. We're going to reset the margin before
            // the animation starts, so we shift these items to the right so that they don't
            // appear to move initially.
            ViewHelper.setTranslationX(mTitle, width);
            ViewHelper.setTranslationX(mSiteSecurity, width);
        } else {
            anim.attach(mTitle,
                        PropertyAnimator.Property.TRANSLATION_X,
                        width);
            anim.attach(mSiteSecurity,
                        PropertyAnimator.Property.TRANSLATION_X,
                        width);
        }
    }

    void finishForwardAnimation() {
        ViewHelper.setTranslationX(mTitle, 0);
        ViewHelper.setTranslationX(mSiteSecurity, 0);
    }

    void prepareStartEditingAnimation() {
        // Hide page actions/stop buttons immediately
        ViewHelper.setAlpha(mPageActionLayout, 0);
        ViewHelper.setAlpha(mStop, 0);
    }

    void prepareStopEditingAnimation(PropertyAnimator anim) {
        // Fade toolbar buttons (page actions, stop) after the entry
        // is shrunk back to its original size.
        anim.attach(mPageActionLayout,
                    PropertyAnimator.Property.ALPHA,
                    1);

        anim.attach(mStop,
                    PropertyAnimator.Property.ALPHA,
                    1);
    }

    boolean dismissSiteIdentityPopup() {
        if (mSiteIdentityPopup != null && mSiteIdentityPopup.isShowing()) {
            mSiteIdentityPopup.dismiss();
            return true;
        }

        return false;
    }

    void destroy() {
        mSiteIdentityPopup.destroy();
    }
}
