/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.animation.ViewHelper;
import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.R;
import org.mozilla.gecko.SiteIdentity;
import org.mozilla.gecko.SiteIdentity.SecurityMode;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.toolbar.BrowserToolbar.ForwardButtonAnimation;
import org.mozilla.gecko.widget.GeckoLinearLayout;
import org.mozilla.gecko.widget.GeckoTextView;

import org.json.JSONObject;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.os.Build;
import android.os.SystemClock;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;
import android.view.animation.AlphaAnimation;
import android.view.animation.TranslateAnimation;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.LinearLayout.LayoutParams;

import java.util.Arrays;
import java.util.List;

public class ToolbarDisplayLayout extends GeckoLinearLayout
                                  implements Animation.AnimationListener {

    private static final String LOGTAG = "GeckoToolbarDisplayLayout";

    final private BrowserApp mActivity;

    private GeckoTextView mTitle;
    private int mTitlePadding;

    private ImageButton mSiteSecurity;
    private boolean mSiteSecurityVisible;
    private boolean mShowSiteSecurity;

    // To de-bounce sets.
    private Bitmap mLastFavicon;
    private ImageButton mFavicon;
    private int mFaviconSize;

    private ImageButton mStop;
    private PageActionLayout mPageActionLayout;

    private Animation mProgressSpinner;
    private boolean mSpinnerVisible;

    private AlphaAnimation mLockFadeIn;
    private TranslateAnimation mTitleSlideLeft;
    private TranslateAnimation mTitleSlideRight;

    private SiteIdentityPopup mSiteIdentityPopup;

    private PropertyAnimator mForwardAnim;

    public ToolbarDisplayLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        setOrientation(HORIZONTAL);

        mActivity = (BrowserApp) context;

        LayoutInflater.from(context).inflate(R.layout.toolbar_display_layout, this);

        mTitle = (GeckoTextView) findViewById(R.id.url_bar_title);
        mTitlePadding = mTitle.getPaddingRight();

        Resources res = getResources();

        mFavicon = (ImageButton) findViewById(R.id.favicon);
        if (Build.VERSION.SDK_INT >= 11) {
            if (Build.VERSION.SDK_INT >= 16) {
                mFavicon.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
            }
            mFavicon.setLayerType(View.LAYER_TYPE_HARDWARE, null);
        }
        mFaviconSize = Math.round(res.getDimension(R.dimen.browser_toolbar_favicon_size));

        mShowSiteSecurity = false;

        mSiteSecurity = (ImageButton) findViewById(R.id.site_security);
        mSiteSecurityVisible = (mSiteSecurity.getVisibility() == View.VISIBLE);

        mSiteIdentityPopup = new SiteIdentityPopup(mActivity);
        mSiteIdentityPopup.setAnchor(mSiteSecurity);

        mProgressSpinner = AnimationUtils.loadAnimation(mActivity, R.anim.progress_spinner);

        mStop = (ImageButton) findViewById(R.id.stop);
        mPageActionLayout = (PageActionLayout) findViewById(R.id.page_action_layout);
    }

    @Override
    public void onAttachedToWindow() {
        Button.OnClickListener faviconListener = new Button.OnClickListener() {
            @Override
            public void onClick(View view) {
                if (mSiteSecurity.getVisibility() != View.VISIBLE)
                    return;

                final Tab tab = Tabs.getInstance().getSelectedTab();

                final SiteIdentity siteIdentity = tab.getSiteIdentity();
                if (siteIdentity.getSecurityMode() == SecurityMode.UNKNOWN) {
                    Log.e(LOGTAG, "Selected tab has no identity data");
                    return;
                }

                mSiteIdentityPopup.updateIdentity(siteIdentity);
                mSiteIdentityPopup.show();
            }
        };

        mFavicon.setOnClickListener(faviconListener);
        mSiteSecurity.setOnClickListener(faviconListener);

        mStop.setOnClickListener(new Button.OnClickListener() {
            @Override
            public void onClick(View v) {
                Tab tab = Tabs.getInstance().getSelectedTab();
                if (tab != null)
                    tab.doStop();
                setProgressVisibility(false);
            }
        });

        float slideWidth = getResources().getDimension(R.dimen.browser_toolbar_lock_width);

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
    public void setPrivateMode(boolean isPrivate) {
        super.setPrivateMode(isPrivate);
        mTitle.setPrivateMode(isPrivate);
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

    void updateFromTab(Tab tab) {
    }

    List<View> getFocusOrder() {
        return Arrays.asList(mSiteSecurity, mPageActionLayout, mStop);
    }

    void setTitle(CharSequence title) {
        mTitle.setText(title);
    }

    void setProgressVisibility(boolean visible) {
        Log.d(LOGTAG, "setProgressVisibility: " + visible);

        // The "Throbber start" and "Throbber stop" log messages in this method
        // are needed by S1/S2 tests (http://mrcote.info/phonedash/#).
        // See discussion in Bug 804457. Bug 805124 tracks paring these down.
        if (visible) {
            mFavicon.setImageResource(R.drawable.progress_spinner);
            mLastFavicon = null;

            //To stop the glitch caused by mutiple start() calls.
            if (!mSpinnerVisible) {
                setPageActionVisibility(true);
                mFavicon.setAnimation(mProgressSpinner);
                mProgressSpinner.start();
                mSpinnerVisible = true;
            }
            Log.i(LOGTAG, "zerdatime " + SystemClock.uptimeMillis() + " - Throbber start");
        } else {
            Tab selectedTab = Tabs.getInstance().getSelectedTab();
            if (selectedTab != null)
                setFavicon(selectedTab.getFavicon());

            if (mSpinnerVisible) {
                setPageActionVisibility(false);
                mFavicon.setAnimation(null);
                mProgressSpinner.cancel();
                mSpinnerVisible = false;
            }
            Log.i(LOGTAG, "zerdatime " + SystemClock.uptimeMillis() + " - Throbber stop");
        }
    }

    void setFavicon(Bitmap image) {
        if (image == mLastFavicon) {
            Log.d(LOGTAG, "Ignoring favicon set: new favicon is identical to previous favicon.");
            return;
        }

        mLastFavicon = image;     // Cache the original so we can debounce without scaling.

        if (image != null) {
            image = Bitmap.createScaledBitmap(image, mFaviconSize, mFaviconSize, false);
            mFavicon.setImageBitmap(image);
        } else {
            mFavicon.setImageDrawable(null);
        }
    }

    private void setSiteSecurityVisibility(final boolean visible, boolean animate) {
        if (visible == mSiteSecurityVisible)
            return;

        mSiteSecurityVisible = visible;

        if (animate) {
            mSiteSecurity.setVisibility(visible ? View.VISIBLE : View.GONE);
            return;
        }

        mTitle.clearAnimation();
        mSiteSecurity.clearAnimation();

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

    void setPageActionVisibility(boolean isLoading) {
        setPageActionVisibility(isLoading, true);
    }

    void setPageActionVisibility(boolean isLoading, boolean animate) {
        // Handle the loading mode page actions
        mStop.setVisibility(isLoading ? View.VISIBLE : View.GONE);

        // Handle the viewing mode page actions
        setSiteSecurityVisibility(mShowSiteSecurity && !isLoading, animate);
        mPageActionLayout.setVisibility(!isLoading ? View.VISIBLE : View.GONE);

        // We want title to fill the whole space available for it when there are icons
        // being shown on the right side of the toolbar as the icons already have some
        // padding in them. This is just to avoid wasting space when icons are shown.
        mTitle.setPadding(0, 0, (!isLoading ? mTitlePadding : 0), 0);
    }

    void setSecurityMode(SecurityMode mode) {
        mSiteSecurity.setImageLevel(mode.ordinal());
        mShowSiteSecurity = (mode != SecurityMode.UNKNOWN);

        setPageActionVisibility(mStop.getVisibility() == View.VISIBLE);
    }

    boolean isShowingProgress() {
        return mSpinnerVisible;
    }

    View getDoorHangerAnchor() {
        return mFavicon;
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
        // is schrunk back to its original size.
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