/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.widget.TabThumbnailWrapper;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.Checkable;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.ImageView.ScaleType;
import android.widget.LinearLayout;
import android.widget.TextView;

public class TabsLayoutItemView extends LinearLayout
                                implements Checkable {
    private static final String LOGTAG = "Gecko" + TabsLayoutItemView.class.getSimpleName();
    private static final int[] STATE_CHECKED = { android.R.attr.state_checked };
    private boolean mChecked;

    private int mTabId;
    private TextView mTitle;
    private ImageView mThumbnail;
    private ImageButton mCloseButton;
    private TabThumbnailWrapper mThumbnailWrapper;

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
        mThumbnail = (ImageView) findViewById(R.id.thumbnail);
        mCloseButton = (ImageButton) findViewById(R.id.close);
        mThumbnailWrapper = (TabThumbnailWrapper) findViewById(R.id.wrapper);
    }

    protected void assignValues(Tab tab)  {
        if (tab == null) {
            return;
        }

        mTabId = tab.getId();

        Drawable thumbnailImage = tab.getThumbnail();
        if (thumbnailImage != null) {
            setThumbnail(thumbnailImage);
        } else {
            mThumbnail.setImageResource(R.drawable.tab_thumbnail_default);
        }
        if (mThumbnailWrapper != null) {
            mThumbnailWrapper.setRecording(tab.isRecording());
        }
        mTitle.setText(tab.getDisplayTitle());
        mCloseButton.setTag(this);
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
}
