/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.menu;

import org.mozilla.gecko.R;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.ViewGroup;
import android.widget.ImageButton;

public class MenuItemActionBar extends ImageButton
                               implements GeckoMenuItem.Layout {
    private static final String LOGTAG = "GeckoMenuItemActionBar";

    private static Bitmap sMoreIcon;
    private static float sHalfIconWidth;
    private static float sMoreWidth;
    private static int sMoreOffset;
    private static Paint sDisabledPaint;

    private Drawable mIcon;
    private boolean mHasSubMenu = false;

    public MenuItemActionBar(Context context) {
        this(context, null);
    }

    public MenuItemActionBar(Context context, AttributeSet attrs) {
        this(context, attrs, R.attr.menuItemActionBarStyle);
    }

    public MenuItemActionBar(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);

        if (sMoreIcon == null) {
            final Resources res = getResources();

            BitmapDrawable drawable  = (BitmapDrawable) res.getDrawable(R.drawable.menu_item_more);
            sMoreIcon = drawable.getBitmap();

            // The icon has some space on the right. Taking half the size feels better.
            sMoreWidth = getResources().getDimensionPixelSize(R.dimen.menu_item_state_icon) / 2.0f;
            sMoreOffset = res.getDimensionPixelSize(R.dimen.menu_item_more_offset);

            final int rowHeight = res.getDimensionPixelSize(R.dimen.menu_item_row_height);
            final int padding = getPaddingTop() + getPaddingBottom();
            sHalfIconWidth = (rowHeight - padding) / 2.0f;

            sDisabledPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
            sDisabledPaint.setColorFilter(new PorterDuffColorFilter(0xFF999999, PorterDuff.Mode.SRC_ATOP));
        }
    }

    @Override
    protected void onDraw(Canvas canvas) {
        if (!mHasSubMenu) {
            super.onDraw(canvas);
            return;
        }

        final int count = canvas.save();

        final float halfWidth = getMeasuredWidth() / 2.0f;
        final float halfHeight = getMeasuredHeight() / 2.0f;

        // If the width is small, the more icon might be pushed to the edges.
        // Instead translate the canvas, so that both the icon + more is centered as a whole.
        final boolean needsTranslation = (halfWidth < 1.5 * halfHeight);
        final float translateX = needsTranslation ? (sMoreOffset + sMoreWidth) / 2.0f : 0.0f;

        canvas.translate(-translateX, 0);

        super.onDraw(canvas);

        final float left = halfWidth + sHalfIconWidth + sMoreOffset - translateX;
        final float top = halfHeight - sMoreWidth;

        canvas.drawBitmap(sMoreIcon, left, top, isEnabled() ? null : sDisabledPaint);

        canvas.translate(translateX, 0);

        canvas.restoreToCount(count);
    }

    @Override
    public void initialize(GeckoMenuItem item) {
        if (item == null)
            return;

        setIcon(item.getIcon());
        setTitle(item.getTitle());
        setEnabled(item.isEnabled());
        setId(item.getItemId());
        setSubMenuIndicator(item.hasSubMenu());
    }

    void setIcon(Drawable icon) {
        mIcon = icon;

        if (icon == null) {
            setVisibility(GONE);
        } else {
            setVisibility(VISIBLE);
            setImageDrawable(icon);
        }
    }

    void setIcon(int icon) {
        setIcon((icon == 0) ? null : getResources().getDrawable(icon));
    }

    void setTitle(CharSequence title) {
        // set accessibility contentDescription here
        setContentDescription(title);
    }

    @Override
    public void setEnabled(boolean enabled) {
        super.setEnabled(enabled);
        setColorFilter(enabled ? 0 : 0xFF999999);
    }

    @Override
    public void setShowIcon(boolean show) {
        // Do nothing.
    }

    private void setSubMenuIndicator(boolean hasSubMenu) {
        if (mHasSubMenu != hasSubMenu) {
            mHasSubMenu = hasSubMenu;
            invalidate();
        }
    }
}
