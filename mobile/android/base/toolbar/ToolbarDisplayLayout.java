/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import java.util.Arrays;
import java.util.EnumSet;
import java.util.List;

import org.mozilla.gecko.AboutPages;
import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.NewTabletUI;
import org.mozilla.gecko.R;
import org.mozilla.gecko.SiteIdentity;
import org.mozilla.gecko.SiteIdentity.SecurityMode;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.animation.ViewHelper;
import org.mozilla.gecko.toolbar.BrowserToolbarTabletBase.ForwardButtonAnimation;
import org.mozilla.gecko.util.StringUtils;
import org.mozilla.gecko.widget.ThemedLinearLayout;
import org.mozilla.gecko.widget.ThemedTextView;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
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
import android.widget.LinearLayout;

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
public class ToolbarDisplayLayout extends ThemedLinearLayout
                                  implements Animation.AnimationListener {

    private static final String LOGTAG = "GeckoToolbarDisplayLayout";

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
    private boolean mSiteSecurityVisible;

    // To de-bounce sets.
    private Bitmap mLastFavicon;
    private final ImageButton mFavicon;
    private int mFaviconSize;

    private final ImageButton mStop;
    private OnStopListener mStopListener;

    private final PageActionLayout mPageActionLayout;

    private AlphaAnimation mLockFadeIn;
    private TranslateAnimation mTitleSlideLeft;
    private TranslateAnimation mTitleSlideRight;

    private final SiteIdentityPopup mSiteIdentityPopup;
    private SecurityMode mSecurityMode;

    private PropertyAnimator mForwardAnim;

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

        mUrlColor = new ForegroundColorSpan(res.getColor(R.color.url_bar_urltext));
        mBlockedColor = new ForegroundColorSpan(res.getColor(R.color.url_bar_blockedtext));
        mDomainColor = new ForegroundColorSpan(res.getColor(R.color.url_bar_domaintext));
        mPrivateDomainColor = new ForegroundColorSpan(res.getColor(R.color.url_bar_domaintext_private));

        mFavicon = (ImageButton) findViewById(R.id.favicon);
        mSiteSecurity = (ImageButton) findViewById(R.id.site_security);

        if (NewTabletUI.isEnabled(context)) {
            mSiteSecurity.setVisibility(View.VISIBLE);
            // TODO: Rename this resource and remove this call when new tablet is default.
            mSiteSecurity.setImageResource(R.drawable.new_tablet_site_security_level);

            // TODO: This can likely be set statically in resources when new tablet is default.
            // Dynamically update parameters for new tablet.
            final LinearLayout.LayoutParams lp =
                    (LinearLayout.LayoutParams) mSiteSecurity.getLayoutParams();
            lp.height = res.getDimensionPixelSize(R.dimen.new_tablet_site_security_height);
            lp.width = res.getDimensionPixelSize(R.dimen.new_tablet_site_security_width);
            // TODO: Override a common static value when new tablet is standard.
            lp.rightMargin = res.getDimensionPixelSize(R.dimen.new_tablet_site_security_right_margin);
            mSiteSecurity.setLayoutParams(lp);
            final int siteSecurityVerticalPadding =
                    res.getDimensionPixelSize(R.dimen.new_tablet_site_security_padding_vertical);
            final int siteSecurityHorizontalPadding =
                    res.getDimensionPixelSize(R.dimen.new_tablet_site_security_padding_horizontal);
            mSiteSecurity.setPadding(siteSecurityHorizontalPadding, siteSecurityVerticalPadding,
                    siteSecurityHorizontalPadding, siteSecurityVerticalPadding);

            // We don't show favicons in the toolbar on new tablet. Note that while we could
            // null the favicon reference, we don't do so to avoid excessive null-checking.
            removeView(mFavicon);
        } else {
            if (Versions.feature16Plus) {
                mFavicon.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
            }
            mFaviconSize = Math.round(res.getDimension(R.dimen.browser_toolbar_favicon_size));
        }

        mSiteSecurityVisible = (mSiteSecurity.getVisibility() == View.VISIBLE);

        mSiteIdentityPopup = new SiteIdentityPopup(mActivity);
        mSiteIdentityPopup.setAnchor(mSiteSecurity);

        mStop = (ImageButton) findViewById(R.id.stop);
        mPageActionLayout = (PageActionLayout) findViewById(R.id.page_action_layout);
    }

    @Override
    public void onAttachedToWindow() {
        mIsAttached = true;

        Button.OnClickListener faviconListener = new Button.OnClickListener() {
            @Override
            public void onClick(View view) {
                if (mSiteSecurity.getVisibility() != View.VISIBLE) {
                    return;
                }

                mSiteIdentityPopup.show();
            }
        };

        mFavicon.setOnClickListener(faviconListener);
        mSiteSecurity.setOnClickListener(faviconListener);

        mStop.setOnClickListener(new Button.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mStopListener != null) {
                    // Force toolbar to switch to Display mode
                    // immediately based on the stopped tab.
                    final Tab tab = mStopListener.onStop();
                    if (tab != null) {
                        updateUiMode(tab, UIMode.DISPLAY, EnumSet.noneOf(UpdateFlags.class));
                    }
                }
            }
        });

        float slideWidth = getResources().getDimension(R.dimen.browser_toolbar_site_security_width);

        LayoutParams siteSecParams = (LayoutParams) mSiteSecurity.getLayoutParams();
        final float scale = getResources().getDisplayMetrics().density;
        slideWidth += (siteSecParams.leftMargin + siteSecParams.rightMargin) * scale + 0.5f;

        mLockFadeIn = new AlphaAnimation(0.0f, 1.0f);
        mLockFadeIn.setAnimationListener(this);

        mTitleSlideLeft = new TranslateAnimation(slideWidth, 0, 0, 0);
        mTitleSlideLeft.setAnimationListener(this);

        mTitleSlideRight = new TranslateAnimation(-slideWidth, 0, 0, 0);
        mTitleSlideRight.setAnimationListener(this);

        final int lockAnimDuration = 300;
        mLockFadeIn.setDuration(lockAnimDuration);
        mTitleSlideLeft.setDuration(lockAnimDuration);
        mTitleSlideRight.setDuration(lockAnimDuration);
    }

    @Override
    public void onDetachedFromWindow() {
        mIsAttached = false;
    }

    @Override
    public void onAnimationStart(Animation animation) {
        if (animation.equals(mLockFadeIn)) {
            if (mSiteSecurityVisible)
                mSiteSecurity.setVisibility(View.VISIBLE);
        } else if (animation.equals(mTitleSlideLeft)) {
            // These two animations may be scheduled to start while the forward
            // animation is occurring. If we're showing the site security icon, make
            // sure it doesn't take any space during the forward transition.
            mSiteSecurity.setVisibility(View.GONE);
        } else if (animation.equals(mTitleSlideRight)) {
            // If we're hiding the icon, make sure that we keep its padding
            // in place during the forward transition
            mSiteSecurity.setVisibility(View.INVISIBLE);
        }
    }

    @Override
    public void onAnimationRepeat(Animation animation) {
    }

    @Override
    public void onAnimationEnd(Animation animation) {
        if (animation.equals(mTitleSlideRight)) {
            mSiteSecurity.startAnimation(mLockFadeIn);
        }
    }

    @Override
    public void setNextFocusDownId(int nextId) {
        mFavicon.setNextFocusDownId(nextId);
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

        if (flags.contains(UpdateFlags.FAVICON)) {
            updateFavicon(tab);
        }

        if (flags.contains(UpdateFlags.SITE_IDENTITY)) {
            updateSiteIdentity(tab, flags);
        }

        if (flags.contains(UpdateFlags.PROGRESS)) {
            updateProgress(tab, flags);
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

        // If the pref to show the URL isn't set, just use the tab's display title.
        if (!mPrefs.shouldShowUrl(mActivity) || url == null) {
            setTitle(tab.getDisplayTitle());
            return;
        }

        CharSequence title = url;
        if (mPrefs.shouldTrimUrls()) {
            title = StringUtils.stripCommonSubdomains(StringUtils.stripScheme(url));
        }

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

    private void updateFavicon(Tab tab) {
        if (NewTabletUI.isEnabled(getContext())) {
            // We don't display favicons in the toolbar for the new Tablet UI.
            return;
        }

        if (tab == null) {
            mFavicon.setImageDrawable(null);
            return;
        }

        Bitmap image = tab.getFavicon();

        if (image != null && image == mLastFavicon) {
            Log.d(LOGTAG, "Ignoring favicon: new image is identical to previous one.");
            return;
        }

        // Cache the original so we can debounce without scaling
        mLastFavicon = image;

        Log.d(LOGTAG, "updateFavicon(" + image + ")");

        if (image != null) {
            image = Bitmap.createScaledBitmap(image, mFaviconSize, mFaviconSize, false);
            mFavicon.setImageBitmap(image);
        } else {
            mFavicon.setImageResource(R.drawable.favicon);
        }
    }

    private void updateSiteIdentity(Tab tab, EnumSet<UpdateFlags> flags) {
        final SiteIdentity siteIdentity;
        if (tab == null) {
            siteIdentity = null;
        } else {
            siteIdentity = tab.getSiteIdentity();
        }

        mSiteIdentityPopup.setSiteIdentity(siteIdentity);

        final SecurityMode securityMode;
        if (siteIdentity == null) {
            securityMode = SecurityMode.UNKNOWN;
        } else {
            securityMode = siteIdentity.getSecurityMode();
        }

        if (mSecurityMode != securityMode) {
            mSecurityMode = securityMode;
            mSiteSecurity.setImageLevel(mSecurityMode.ordinal());
            updatePageActions(flags);
        }
    }

    private void updateProgress(Tab tab, EnumSet<UpdateFlags> flags) {
        final boolean shouldShowThrobber = (tab != null &&
                                            tab.getState() == Tab.STATE_LOADING);

        updateUiMode(tab, shouldShowThrobber ? UIMode.PROGRESS : UIMode.DISPLAY, flags);
    }

    private void updateUiMode(Tab tab, UIMode uiMode, EnumSet<UpdateFlags> flags) {
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

        updatePageActions(flags);
    }

    private void updatePageActions(EnumSet<UpdateFlags> flags) {
        final boolean isShowingProgress = (mUiMode == UIMode.PROGRESS);

        mStop.setVisibility(isShowingProgress ? View.VISIBLE : View.GONE);
        mPageActionLayout.setVisibility(!isShowingProgress ? View.VISIBLE : View.GONE);

        boolean shouldShowSiteSecurity = (!isShowingProgress &&
                                          mSecurityMode != SecurityMode.UNKNOWN);

        setSiteSecurityVisibility(shouldShowSiteSecurity, flags);

        // We want title to fill the whole space available for it when there are icons
        // being shown on the right side of the toolbar as the icons already have some
        // padding in them. This is just to avoid wasting space when icons are shown.
        mTitle.setPadding(0, 0, (!isShowingProgress ? mTitlePadding : 0), 0);
    }

    private void setSiteSecurityVisibility(boolean visible, EnumSet<UpdateFlags> flags) {
        // We don't hide site security on new tablets.
        if (visible == mSiteSecurityVisible || NewTabletUI.isEnabled(getContext())) {
            return;
        }

        mSiteSecurityVisible = visible;

        mTitle.clearAnimation();
        mSiteSecurity.clearAnimation();

        if (flags.contains(UpdateFlags.DISABLE_ANIMATIONS)) {
            mSiteSecurity.setVisibility(visible ? View.VISIBLE : View.GONE);
            return;
        }

        // If any of these animations were cancelled as a result of the
        // clearAnimation() calls above, we need to reset them.
        mLockFadeIn.reset();
        mTitleSlideLeft.reset();
        mTitleSlideRight.reset();

        if (mForwardAnim != null) {
            long delay = mForwardAnim.getRemainingTime();
            mTitleSlideRight.setStartOffset(delay);
            mTitleSlideLeft.setStartOffset(delay);
        } else {
            mTitleSlideRight.setStartOffset(0);
            mTitleSlideLeft.setStartOffset(0);
        }

        mTitle.startAnimation(visible ? mTitleSlideRight : mTitleSlideLeft);
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

    View getDoorHangerAnchor() {
        if (!NewTabletUI.isEnabled(getContext())) {
            return mFavicon;
        } else {
            return mSiteSecurity;
        }
    }

    void prepareForwardAnimation(PropertyAnimator anim, ForwardButtonAnimation animation, int width) {
        mForwardAnim = anim;

        if (animation == ForwardButtonAnimation.HIDE) {
            anim.attach(mTitle,
                        PropertyAnimator.Property.TRANSLATION_X,
                        0);
            anim.attach(mFavicon,
                        PropertyAnimator.Property.TRANSLATION_X,
                        0);
            anim.attach(mSiteSecurity,
                        PropertyAnimator.Property.TRANSLATION_X,
                        0);

            // We're hiding the forward button. We're going to reset the margin before
            // the animation starts, so we shift these items to the right so that they don't
            // appear to move initially.
            ViewHelper.setTranslationX(mTitle, width);
            ViewHelper.setTranslationX(mFavicon, width);
            ViewHelper.setTranslationX(mSiteSecurity, width);
        } else {
            anim.attach(mTitle,
                        PropertyAnimator.Property.TRANSLATION_X,
                        width);
            anim.attach(mFavicon,
                        PropertyAnimator.Property.TRANSLATION_X,
                        width);
            anim.attach(mSiteSecurity,
                        PropertyAnimator.Property.TRANSLATION_X,
                        width);
        }
    }

    void finishForwardAnimation() {
        ViewHelper.setTranslationX(mTitle, 0);
        ViewHelper.setTranslationX(mFavicon, 0);
        ViewHelper.setTranslationX(mSiteSecurity, 0);

        mForwardAnim = null;
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
}
