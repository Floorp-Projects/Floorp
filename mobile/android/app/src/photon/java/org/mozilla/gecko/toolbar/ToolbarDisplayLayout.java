/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import java.util.Arrays;
import java.util.EnumSet;
import java.util.List;

import android.support.v4.content.ContextCompat;
import org.mozilla.gecko.AboutPages;
import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.R;
import org.mozilla.gecko.reader.ReaderModeUtils;
import org.mozilla.gecko.SiteIdentity;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.animation.ViewHelper;
import org.mozilla.gecko.util.StringUtils;
import org.mozilla.gecko.util.ViewUtil;
import org.mozilla.gecko.widget.themed.ThemedImageButton;
import org.mozilla.gecko.widget.themed.ThemedLinearLayout;
import org.mozilla.gecko.widget.themed.ThemedTextView;

import android.content.Context;
import android.support.annotation.NonNull;
import android.text.Editable;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.text.style.ForegroundColorSpan;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.HorizontalScrollView;

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

    private static final int MIN_DOMAIN_SCROLL_MARGIN_DP = 10;

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
        Tab onStop();
    }

    interface OnTitleChangeListener {
        void onTitleChange(CharSequence title);
    }

    private final BrowserApp mActivity;

    private UIMode mUiMode;

    private boolean mIsAttached;

    private final ThemedTextView mTitle;
    private final ThemedLinearLayout mThemeBackground;
    private final int mTitlePadding;
    private final HorizontalScrollView mTitleScroll;
    private final int mMinUrlScrollMargin;
    private ToolbarPrefs mPrefs;
    private OnTitleChangeListener mTitleChangeListener;

    private final ThemedImageButton mSiteSecurity;
    private final ThemedImageButton mStop;
    private OnStopListener mStopListener;

    private final PageActionLayout mPageActionLayout;

    private final SiteIdentityPopup mSiteIdentityPopup;
    private int mSecurityImageLevel;

    private final ForegroundColorSpan mUrlColorSpan;
    private final ForegroundColorSpan mPrivateUrlColorSpan;
    private final ForegroundColorSpan mBlockedColorSpan;
    private final ForegroundColorSpan mPrivateBlockedColorSpan;
    private final ForegroundColorSpan mDomainColorSpan;
    private final ForegroundColorSpan mPrivateDomainColorSpan;

    public ToolbarDisplayLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        setOrientation(HORIZONTAL);

        mActivity = (BrowserApp) context;

        LayoutInflater.from(context).inflate(R.layout.toolbar_display_layout, this);

        mTitle = (ThemedTextView) findViewById(R.id.url_bar_title);
        mThemeBackground = (ThemedLinearLayout) findViewById(R.id.url_bar_title_bg);
        mTitlePadding = mTitle.getPaddingRight();
        mTitleScroll = (HorizontalScrollView) findViewById(R.id.url_bar_title_scroll_view);

        final OnLayoutChangeListener resizeListener = new OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft, int oldTop, int oldRight, int oldBottom) {
                final int oldWidth = oldRight - oldLeft;
                final int newWidth = right - left;

                if (newWidth != oldWidth) {
                    scrollTitle();
                }
            }
        };
        mTitle.addTextChangedListener(new TextChangeListener());
        mTitle.addOnLayoutChangeListener(resizeListener);
        mTitleScroll.addOnLayoutChangeListener(resizeListener);

        mMinUrlScrollMargin = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
                                                              MIN_DOMAIN_SCROLL_MARGIN_DP,
                                                              getResources().getDisplayMetrics());

        mUrlColorSpan = new ForegroundColorSpan(ContextCompat.getColor(context, R.color.url_bar_urltext));
        mPrivateUrlColorSpan = new ForegroundColorSpan(ContextCompat.getColor(context, R.color.url_bar_urltext_private));
        mBlockedColorSpan = new ForegroundColorSpan(ContextCompat.getColor(context, R.color.url_bar_blockedtext));
        mPrivateBlockedColorSpan = new ForegroundColorSpan(ContextCompat.getColor(context, R.color.url_bar_blockedtext_private));
        mDomainColorSpan = new ForegroundColorSpan(ContextCompat.getColor(context, R.color.url_bar_domaintext));
        mPrivateDomainColorSpan = new ForegroundColorSpan(ContextCompat.getColor(context, R.color.url_bar_domaintext_private));

        mSiteSecurity = (ThemedImageButton) findViewById(R.id.site_security);

        mSiteIdentityPopup = new SiteIdentityPopup(mActivity);
        mSiteIdentityPopup.setAnchor(this);
        mSiteIdentityPopup.setOnVisibilityChangeListener(mActivity);

        mStop = (ThemedImageButton) findViewById(R.id.stop);
        mPageActionLayout = (PageActionLayout) findViewById(R.id.page_action_layout);
    }

    @Override
    public void setPrivateMode(boolean isPrivate) {
        super.setPrivateMode(isPrivate);
        mSiteSecurity.setPrivateMode(isPrivate);
        mStop.setPrivateMode(isPrivate);
        mPageActionLayout.setPrivateMode(isPrivate);
        mTitle.setPrivateMode(isPrivate);
        mThemeBackground.setPrivateMode(isPrivate);
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();

        mIsAttached = true;

        final Tab selectedTab = Tabs.getInstance().getSelectedTab();
        if (selectedTab != null) {
            // A tab was selected before we became ready; update to that tab now.
            updateFromTab(selectedTab, EnumSet.allOf(UpdateFlags.class));
        }

        mSiteIdentityPopup.registerListeners();

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
        mSiteIdentityPopup.unregisterListeners();
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

    void updateFromTab(@NonNull Tab tab, EnumSet<UpdateFlags> flags) {
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
            mTitle.setPrivateMode(tab.isPrivate());
            mThemeBackground.setPrivateMode(tab.isPrivate());
        }
    }

    void setTitle(CharSequence title) {
        mTitle.setText(title);

        if (TextUtils.isEmpty(title)) {
            //  Reset TextDirection to Locale in order to reveal text hint in correct direction
            ViewUtil.setTextDirection(mTitle, TEXT_DIRECTION_LOCALE);
        } else {
            //  Otherwise, fall back to default first strong strategy
            ViewUtil.setTextDirection(mTitle, TEXT_DIRECTION_FIRST_STRONG);
        }

        if (mTitleChangeListener != null) {
            mTitleChangeListener.onTitleChange(title);
        }
    }

    private void updateTitle(@NonNull Tab tab) {
        // Keep the title unchanged if there's no selected tab,
        // or if the tab is entering reader mode.
        if (tab.isEnteringReaderMode()) {
            return;
        }

        final String url = tab.getURL();

        // Setting a null title will ensure we just see the
        // "Enter Search or Address" placeholder text.
        if (AboutPages.isTitlelessAboutPage(url)) {
            setTitle(null);
            setContentDescription(null);
            return;
        }

        // Show the about:blocked page title in red, regardless of prefs
        if (tab.getErrorType() == Tab.ErrorType.BLOCKED) {
            final String title = tab.getDisplayTitle();

            final SpannableStringBuilder builder = new SpannableStringBuilder(title);
            final ForegroundColorSpan fgColorSpan = tab.isPrivate()
                    ? mPrivateBlockedColorSpan
                    : mBlockedColorSpan;
            builder.setSpan(fgColorSpan, 0, title.length(), Spannable.SPAN_INCLUSIVE_INCLUSIVE);

            setTitle(builder);
            setContentDescription(null);
            return;
        }

        final String baseDomain = tab.getBaseDomain();

        String strippedURL = stripAboutReaderURL(url);

        if (mPrefs.shouldTrimUrls()) {
            strippedURL = StringUtils.stripCommonSubdomains(StringUtils.stripScheme(strippedURL));
        }

        // The URL bar does not support RTL currently (See bug 928688 and meta bug 702845).
        // Displaying a URL using RTL (or mixed) characters can lead to an undesired reordering
        // of elements of the URL. That's why we are forcing the URL to use LTR (bug 1284372).
        strippedURL = StringUtils.forceLTR(strippedURL);

        // This value is not visible to screen readers but we rely on it when running UI tests. Screen
        // readers will instead focus BrowserToolbar and read the "base domain" from there. UI tests
        // will read the content description to obtain the full URL for performing assertions.
        setContentDescription(strippedURL);

        // Display full URL with base domain highlighted as title
        updateAndColorTitleFromFullURL(strippedURL, baseDomain, tab.isPrivate());
    }

    private void updateAndColorTitleFromFullURL(String url, String baseDomain, boolean isPrivate) {
        if (TextUtils.isEmpty(baseDomain)) {
            setTitle(url);
            return;
        }

        int index = url.indexOf(baseDomain);
        if (index == -1) {
            setTitle(url);
            return;
        }

        final SpannableStringBuilder builder = new SpannableStringBuilder(url);

        builder.setSpan(isPrivate ? mPrivateUrlColorSpan : mUrlColorSpan, 0, url.length(), Spannable.SPAN_INCLUSIVE_INCLUSIVE);
        builder.setSpan(isPrivate ? mPrivateDomainColorSpan : mDomainColorSpan,
                index, index + baseDomain.length(), Spannable.SPAN_INCLUSIVE_INCLUSIVE);

        setTitle(builder);
    }

    private String stripAboutReaderURL(final String url) {
        if (!AboutPages.isAboutReader(url)) {
            return url;
        }

        return ReaderModeUtils.stripAboutReaderUrl(url);
    }

    private void updateSiteIdentity(@NonNull Tab tab) {
        final SiteIdentity siteIdentity = tab.getSiteIdentity();
        final SecurityModeUtil.IconType type = SecurityModeUtil.resolve(siteIdentity, tab.getURL());
        final int imageLevel = type.getImageLevel();

        mSiteIdentityPopup.setSiteIdentity(siteIdentity);
        mTrackingProtectionEnabled = SecurityModeUtil.isTrackingProtectionEnabled(siteIdentity);

        if (mSecurityImageLevel != imageLevel) {
            mSecurityImageLevel = imageLevel;
            mSiteSecurity.setImageLevel(mSecurityImageLevel);
            updatePageActions();
        }
    }

    private void scrollTitle() {
        final Editable text = mTitle.getEditableText();
        final int textViewWidth = mTitle.getWidth();
        final int textWidth = textViewWidth - mTitlePadding;
        final int scrollViewWidth = mTitleScroll.getWidth();
        if (textWidth <= scrollViewWidth) {
            // The text fits within the ScrollView, so nothing to do here...
            if (textViewWidth > scrollViewWidth) {
                // ... although if the TextView is sufficiently padded on the right side, it might
                // push the text out of view on the left side, so scroll to the beginning just to be
                // on the safe side.
                mTitleScroll.scrollTo(0, 0);
            }
            return;
        }

        final ForegroundColorSpan spanToCheck =
                mTitle.isPrivateMode() ? mPrivateDomainColorSpan : mDomainColorSpan;
        final int domainEnd = text != null ? text.getSpanEnd(spanToCheck) : -1;
        if (domainEnd == -1) {
            // We're not showing a domain, just scroll to the start of the text.
            mTitleScroll.scrollTo(0, 0);
            return;
        }

        // If we're showing an URL that is larger than the URL bar, we want to align the end of
        // the domain part with the right side of URL bar, so as to put the focus on the base
        // domain and avoid phishing attacks using long subdomains that have been crafted to be cut
        // off at just the right place and then resemble a legitimate base domain.
        final int domainTextWidth = StringUtils.getTextWidth(text.toString(), 0, domainEnd, mTitle.getPaint());
        final int overhang = textViewWidth - domainTextWidth;
        // For optimal alignment, we want to take the fadingEdge into account and align the domain
        // with the start of the fade out.
        final int maxFadingEdge = mTitleScroll.getHorizontalFadingEdgeLength();

        // The width of the fadingEdge corresponds to the width of the child view that is overhanging
        // the ScrollView, clamped by maxFadingEdge.
        int targetMargin = overhang / 2;
        targetMargin = Math.min(targetMargin, maxFadingEdge);
        // Even when there is no fadingEdge, we want to keep a little margin between the domain and
        // the end of the URL bar, so as to show the first character or so of the path part.
        targetMargin = Math.max(targetMargin, mMinUrlScrollMargin);

        final int scrollTarget = domainTextWidth + targetMargin - scrollViewWidth;
        mTitleScroll.scrollTo(scrollTarget, 0);
    }

    private void updateProgress(@NonNull Tab tab) {
        final boolean shouldShowThrobber = tab.getState() == Tab.STATE_LOADING;

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

    List<? extends View> getFocusOrder() {
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

    private class TextChangeListener implements TextWatcher {
        @Override
        public void afterTextChanged(Editable text) {
            scrollTitle();
        }

        @Override
        public void beforeTextChanged(CharSequence s, int start, int count, int after) { }

        @Override
        public void onTextChanged(CharSequence s, int start, int before, int count) { }
    }
}
