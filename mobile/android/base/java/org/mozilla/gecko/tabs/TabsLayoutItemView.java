/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.MotionEvent;
import android.view.View;
import android.widget.Checkable;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.Icons;
import org.mozilla.gecko.util.ViewUtil;
import org.mozilla.gecko.widget.FaviconView;
import org.mozilla.gecko.widget.HoverDelegateWithReset;
import org.mozilla.gecko.widget.TabThumbnailWrapper;
import org.mozilla.gecko.widget.TouchDelegateWithReset;
import org.mozilla.gecko.widget.themed.ThemedRelativeLayout;

import java.util.concurrent.Future;

public class TabsLayoutItemView extends LinearLayout
                                implements Checkable {
    private static final String LOGTAG = "Gecko" + TabsLayoutItemView.class.getSimpleName();
    private static final int[] STATE_CHECKED = { android.R.attr.state_checked };
    private boolean mChecked;

    private int mTabId;
    private TextView mTitle;
    private TabsPanelThumbnailView mThumbnail;
    private ImageView mCloseButton;
    private TabThumbnailWrapper mThumbnailWrapper;
    private HoverDelegateWithReset mHoverDelegate;

    private FaviconView mFaviconView;
    private Future<IconResponse> mOngoingIconLoad;

    public TabsLayoutItemView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public int[] onCreateDrawableState(int extraSpace) {
        final int[] drawableState = super.onCreateDrawableState(extraSpace + 1);

        if (mChecked) {
            mergeDrawableStates(drawableState, STATE_CHECKED);
        }

        return drawableState;
    }

    @Override
    public boolean isEnabled() {
        return true;
    }

    @Override
    public boolean isChecked() {
        return mChecked;
    }

    @Override
    public void setChecked(boolean checked) {
        if (mChecked == checked) {
            return;
        }

        mChecked = checked;
        refreshDrawableState();

        int count = getChildCount();
        for (int i = 0; i < count; i++) {
            final View child = getChildAt(i);
            if (child instanceof Checkable) {
                ((Checkable) child).setChecked(checked);
            }
        }
    }

    @Override
    public void toggle() {
        mChecked = !mChecked;
    }

    public void setCloseOnClickListener(OnClickListener mOnClickListener) {
        mCloseButton.setOnClickListener(mOnClickListener);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitle = (TextView) findViewById(R.id.title);
        mThumbnail = (TabsPanelThumbnailView) findViewById(R.id.thumbnail);
        mCloseButton = (ImageView) findViewById(R.id.close);
        mThumbnailWrapper = (TabThumbnailWrapper) findViewById(R.id.wrapper);
        mFaviconView = (FaviconView) findViewById(R.id.favicon);

        growCloseButtonHitArea();
    }

    private void growCloseButtonHitArea() {
        addOnLayoutChangeListener(new OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft, int oldTop, int oldRight, int oldBottom) {
                // Ideally we want the close button hit area to be 40x40dp but we are constrained by the height of the parent, so
                // we make it as tall as the parent view and 40dp across.
                final int targetHitArea = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 40, getResources().getDisplayMetrics());

                final Rect hitRect = getHitRectRelatively(targetHitArea);

                setTouchDelegate(new TouchDelegateWithReset(hitRect, mCloseButton));
                setHoverDelegate(new HoverDelegateWithReset(hitRect, mCloseButton));
            }
        });
    }

    private Rect getHitRectRelatively(int targetHitArea) {
        final boolean isRtl = ViewUtil.isLayoutRtl(this);
        final Rect hitRect = new Rect();
        hitRect.top = 0;
        hitRect.right = isRtl ? targetHitArea : getWidth();
        hitRect.left = isRtl ? 0 : getWidth() - targetHitArea;
        hitRect.bottom = targetHitArea;
        return hitRect;
    }

    /**
     * Sets the HoverDelegate for this View.
     */
    public void setHoverDelegate(HoverDelegateWithReset delegate) {
        mHoverDelegate = delegate;
    }

    @Override
    public boolean onHoverEvent(MotionEvent event) {
        if (mHoverDelegate != null) {
            if (mHoverDelegate.onHoverEvent(event)) {
                return true;
            }
        }

        return super.onHoverEvent(event);
    }

    protected void assignValues(Tab tab)  {
        if (tab == null) {
            return;
        }

        mTabId = tab.getId();

        setChecked(Tabs.getInstance().isSelectedTab(tab));

        Drawable thumbnailImage = tab.getThumbnail();
        mThumbnail.setImageDrawable(thumbnailImage);

        mThumbnail.setPrivateMode(tab.isPrivate());

        if (mThumbnailWrapper != null) {
            mThumbnailWrapper.setRecording(tab.isRecording());
        }

        final String tabTitle = tab.getDisplayTitle();
        mTitle.setText(tabTitle);
        mCloseButton.setTag(this);

        if (tab.isAudioPlaying()) {
            mFaviconView.setImageResource(R.drawable.tab_audio_playing);
            final String tabTitleWithAudio =
                    getResources().getString(R.string.tab_title_prefix_is_playing_audio, tabTitle);
            mTitle.setContentDescription(tabTitleWithAudio);
        } else {
            final String url = tab.getURL();
            if (TextUtils.isEmpty(url)) {
                // Ignore loading favicon without url.
                return;
            }

            if (mOngoingIconLoad != null) {
                mOngoingIconLoad.cancel(true);
            }

            final Resources resources = getResources();
            final int iconSize = resources.getDimensionPixelSize(R.dimen.tab_favicon_size);
            final float textSize = resources.getDimensionPixelSize(R.dimen.tab_favicon_text_size);

            final Context appContext = getContext().getApplicationContext();
            mOngoingIconLoad = Icons.with(appContext)
                                       .pageUrl(url)
                                       .setPrivateMode(tab.isPrivate())
                                       .skipNetwork()
                                       .targetSize(iconSize)
                                       .textSize(textSize)
                                       .build()
                                       .execute(mFaviconView.createIconCallback());

            mTitle.setContentDescription(tabTitle);
        }
    }

    public int getTabId() {
        return mTabId;
    }

    public void setThumbnail(Drawable thumbnail) {
        mThumbnail.setImageDrawable(thumbnail);
    }

    public void setCloseVisible(boolean visible) {
        mCloseButton.setVisibility(visible ? View.VISIBLE : View.INVISIBLE);
    }

    public void setPrivateMode(boolean isPrivate) {
        ((ThemedRelativeLayout) findViewById(R.id.wrapper)).setPrivateMode(isPrivate);
    }
}
