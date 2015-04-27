/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import java.util.Arrays;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.animation.ViewHelper;
import org.mozilla.gecko.widget.ThemedImageView;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.util.AttributeSet;
import android.view.View;
import android.view.animation.AccelerateInterpolator;
import android.view.animation.Interpolator;
import android.widget.ImageView;

/**
 * A base implementations of the browser toolbar for phones.
 * This class manages any Views, variables, etc. that are exclusive to phone.
 */
abstract class BrowserToolbarPhoneBase extends BrowserToolbar {

    protected final ImageView urlBarTranslatingEdge;
    protected final ThemedImageView editCancel;

    private final Path roundCornerShape;
    private final Paint roundCornerPaint;

    private final Interpolator buttonsInterpolator = new AccelerateInterpolator();

    public BrowserToolbarPhoneBase(final Context context, final AttributeSet attrs) {
        super(context, attrs);
        final Resources res = context.getResources();

        urlBarTranslatingEdge = (ImageView) findViewById(R.id.url_bar_translating_edge);

        // This will clip the translating edge's image at 60% of its width
        urlBarTranslatingEdge.getDrawable().setLevel(6000);

        editCancel = (ThemedImageView) findViewById(R.id.edit_cancel);

        focusOrder.add(this);
        focusOrder.addAll(urlDisplayLayout.getFocusOrder());
        focusOrder.addAll(Arrays.asList(tabsButton, menuButton));

        roundCornerShape = new Path();
        roundCornerShape.moveTo(0, 0);
        roundCornerShape.lineTo(30, 0);
        roundCornerShape.cubicTo(0, 0, 0, 0, 0, 30);
        roundCornerShape.lineTo(0, 0);

        roundCornerPaint = new Paint();
        roundCornerPaint.setAntiAlias(true);
        roundCornerPaint.setColor(res.getColor(R.color.text_and_tabs_tray_grey));
        roundCornerPaint.setStrokeWidth(0.0f);
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();

        editCancel.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                // If we exit editing mode during the animation,
                // we're put into an inconsistent state (bug 1017276).
                if (!isAnimating()) {
                    Telemetry.sendUIEvent(TelemetryContract.Event.CANCEL,
                            TelemetryContract.Method.ACTIONBAR,
                            getResources().getResourceEntryName(editCancel.getId()));
                    cancelEdit();
                }
            }
        });
    }

    @Override
    public void setPrivateMode(final boolean isPrivate) {
        super.setPrivateMode(isPrivate);
        editCancel.setPrivateMode(isPrivate);
    }

    @Override
    protected boolean isTabsButtonOffscreen() {
        return isEditing();
    }

    @Override
    public boolean addActionItem(final View actionItem) {
        // We have no action item bar.
        return false;
    }

    @Override
    public void removeActionItem(final View actionItem) {
        // We have no action item bar.
    }

    @Override
    protected void updateNavigationButtons(final Tab tab) {
        // We have no navigation buttons so do nothing.
    }

    @Override
    public void draw(final Canvas canvas) {
        super.draw(canvas);

        if (uiMode == UIMode.DISPLAY) {
            canvas.drawPath(roundCornerShape, roundCornerPaint);
        }
    }

    @Override
    public void triggerTabsPanelTransition(final PropertyAnimator animator, final boolean areTabsShown) {
        if (areTabsShown) {
            ViewHelper.setAlpha(tabsCounter, 0.0f);
            if (hasSoftMenuButton) {
                ViewHelper.setAlpha(menuIcon, 0.0f);
            }
            return;
        }

        final PropertyAnimator buttonsAnimator =
                new PropertyAnimator(animator.getDuration(), buttonsInterpolator);

        buttonsAnimator.attach(tabsCounter,
                               PropertyAnimator.Property.ALPHA,
                               1.0f);
        if (hasSoftMenuButton) {
            buttonsAnimator.attach(menuIcon,
                                   PropertyAnimator.Property.ALPHA,
                                   1.0f);
        }

        buttonsAnimator.start();
    }

    /**
     * Returns the number of pixels the url bar translating edge
     * needs to translate to the right to enter its editing mode state.
     * A negative value means the edge must translate to the left.
     */
    protected int getUrlBarEntryTranslation() {
        // Find the distance from the right-edge of the url bar (where we're translating from) to
        // the left-edge of the cancel button (where we're translating to; note that the cancel
        // button must be laid out, i.e. not View.GONE).
        return editCancel.getLeft() - urlBarEntry.getRight();
    }

    protected int getUrlBarCurveTranslation() {
        return getWidth() - tabsButton.getLeft();
    }

    protected void updateTabCountAndAnimate(final int count) {
        // Don't animate if the toolbar is hidden.
        if (!isVisible()) {
            updateTabCount(count);
            return;
        }

        // If toolbar is in edit mode on a phone, this means the entry is expanded
        // and the tabs button is translated offscreen. Don't trigger tabs counter
        // updates until the tabs button is back on screen.
        // See stopEditing()
        if (!isTabsButtonOffscreen()) {
            tabsCounter.setCount(count);

            tabsButton.setContentDescription((count > 1) ?
                                             activity.getString(R.string.num_tabs, count) :
                                             activity.getString(R.string.one_tab));
        }
    }

    @Override
    protected void setUrlEditLayoutVisibility(final boolean showEditLayout,
            final PropertyAnimator animator) {
        super.setUrlEditLayoutVisibility(showEditLayout, animator);

        if (animator == null) {
            editCancel.setVisibility(showEditLayout ? View.VISIBLE : View.INVISIBLE);
            return;
        }

        animator.addPropertyAnimationListener(new PropertyAnimator.PropertyAnimationListener() {
            @Override
            public void onPropertyAnimationStart() {
                if (!showEditLayout) {
                    editCancel.setVisibility(View.INVISIBLE);
                }
            }

            @Override
            public void onPropertyAnimationEnd() {
                if (showEditLayout) {
                    editCancel.setVisibility(View.VISIBLE);
                }
            }
        });
    }

    @Override
    public void onLightweightThemeChanged() {
        super.onLightweightThemeChanged();
        editCancel.onLightweightThemeChanged();
    }

    @Override
    public void onLightweightThemeReset() {
        super.onLightweightThemeReset();
        editCancel.onLightweightThemeReset();
    }
}
