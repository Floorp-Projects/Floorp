/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Paint;
import org.mozilla.gecko.animation.AnimationUtils;
import org.mozilla.gecko.menu.GeckoMenu;
import org.mozilla.gecko.widget.GeckoPopupMenu;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.Animation;
import android.view.animation.ScaleAnimation;
import android.view.animation.TranslateAnimation;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.LinearLayout;

class ActionModeCompatView extends LinearLayout implements GeckoMenu.ActionItemBarPresenter {
    private final String LOGTAG = "GeckoActionModeCompatPresenter";

    private static final int SPEC = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED);

    private Button mTitleView;
    private ImageButton mMenuButton;
    private ViewGroup mActionButtonBar;
    private GeckoPopupMenu mPopupMenu;

    // Maximum number of items to show as actions
    private static final int MAX_ACTION_ITEMS = 4;

    private int mActionButtonsWidth;

    private Paint mBottomDividerPaint;
    private int mBottomDividerOffset;

    public ActionModeCompatView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init(context, attrs, 0);
    }

    public ActionModeCompatView(Context context, AttributeSet attrs, int style) {
        super(context, attrs, style);
        init(context, attrs, style);
    }

    public void init(final Context context, final AttributeSet attrs, final int defStyle) {
        LayoutInflater.from(context).inflate(R.layout.actionbar, this);

        mTitleView = (Button) findViewById(R.id.actionmode_title);
        mMenuButton = (ImageButton) findViewById(R.id.actionbar_menu);
        mActionButtonBar = (ViewGroup) findViewById(R.id.actionbar_buttons);

        mPopupMenu = new GeckoPopupMenu(getContext(), mMenuButton);
        mPopupMenu.getMenu().setActionItemBarPresenter(this);

        mMenuButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                openMenu();
            }
        });

        // The built-in action bar uses colorAccent for the divider so we duplicate that here.
        final TypedArray arr = context.obtainStyledAttributes(attrs, new int[] { R.attr.colorAccent }, defStyle, 0);
        final int bottomDividerColor = arr.getColor(0, 0);
        arr.recycle();

        mBottomDividerPaint = new Paint();
        mBottomDividerPaint.setColor(bottomDividerColor);
        mBottomDividerOffset = getResources().getDimensionPixelSize(R.dimen.action_bar_divider_height);
    }

    public void initForMode(final ActionModeCompat mode) {
        mTitleView.setOnClickListener(mode);
        mPopupMenu.setOnMenuItemClickListener(mode);
        mPopupMenu.setOnMenuItemLongClickListener(mode);
    }

    public CharSequence getTitle() {
        return mTitleView.getText();
    }

    public void setTitle(CharSequence title) {
        mTitleView.setText(title);
    }

    public void setTitle(int resId) {
        mTitleView.setText(resId);
    }

    public GeckoMenu getMenu() {
        return mPopupMenu.getMenu();
    }

    @Override
    public void invalidate() {
        // onFinishInflate may not have been called yet on some versions of Android
        if (mPopupMenu != null && mMenuButton != null) {
            mMenuButton.setVisibility(mPopupMenu.getMenu().hasVisibleItems() ? View.VISIBLE : View.GONE);
        }
        super.invalidate();
    }

    /* GeckoMenu.ActionItemBarPresenter */
    @Override
    public boolean addActionItem(View actionItem) {
        final int count = mActionButtonBar.getChildCount();
        if (count >= MAX_ACTION_ITEMS) {
            return false;
        }

        int maxWidth = mActionButtonBar.getMeasuredWidth();
        if (maxWidth == 0) {
            mActionButtonBar.measure(SPEC, SPEC);
            maxWidth = mActionButtonBar.getMeasuredWidth();
        }

        // If the menu button is already visible, no need to account for it
        if (mMenuButton.getVisibility() == View.GONE) {
            // Since we don't know how many items will be added, we always reserve space for the overflow menu
            mMenuButton.measure(SPEC, SPEC);
            maxWidth -= mMenuButton.getMeasuredWidth();
        }

        if (mActionButtonsWidth <= 0) {
            mActionButtonsWidth = 0;

            // Loop over child views, measure them, and add their width to the taken width
            for (int i = 0; i < count; i++) {
                View v = mActionButtonBar.getChildAt(i);
                v.measure(SPEC, SPEC);
                mActionButtonsWidth += v.getMeasuredWidth();
            }
        }

        actionItem.measure(SPEC, SPEC);
        int w = actionItem.getMeasuredWidth();
        if (mActionButtonsWidth + w < maxWidth) {
            // We cache the new width of our children.
            mActionButtonsWidth += w;
            mActionButtonBar.addView(actionItem);
            return true;
        }

        return false;
    }

    /* GeckoMenu.ActionItemBarPresenter */
    @Override
    public void removeActionItem(View actionItem) {
        actionItem.measure(SPEC, SPEC);
        mActionButtonsWidth -= actionItem.getMeasuredWidth();
        mActionButtonBar.removeView(actionItem);
    }

    public void openMenu() {
        mPopupMenu.openMenu();
    }

    public void closeMenu() {
        mPopupMenu.dismiss();
    }

    public void animateIn() {
        long duration = AnimationUtils.getShortDuration(getContext());
        TranslateAnimation t = new TranslateAnimation(Animation.RELATIVE_TO_SELF, -0.5f, Animation.RELATIVE_TO_SELF, 0f,
                                                      Animation.RELATIVE_TO_SELF,  0f,   Animation.RELATIVE_TO_SELF, 0f);
        t.setDuration(duration);

        ScaleAnimation s = new ScaleAnimation(1f, 1f, 0f, 1f,
                                              Animation.RELATIVE_TO_SELF, 0.5f, Animation.RELATIVE_TO_SELF, 0.5f);
        s.setDuration((long) (duration * 1.5f));

        mTitleView.startAnimation(t);
        mActionButtonBar.startAnimation(s);

        if ((mMenuButton.getVisibility() == View.VISIBLE) &&
            (mPopupMenu.getMenu().size() > 0)) {
            mMenuButton.startAnimation(s);
        }
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        // Draw the divider at the bottom of the screen. We could do this with a layer-list
        // but then we'd have overdraw (http://stackoverflow.com/a/13509472).
        final int bottom = getHeight();
        final int top = bottom - mBottomDividerOffset;
        canvas.drawRect(0, top, getWidth(), bottom, mBottomDividerPaint);
    }
}
