/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import java.util.Arrays;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.Tabs.TabEvents;
import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.animation.ViewHelper;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.view.animation.AccelerateInterpolator;
import android.view.animation.Interpolator;
import android.widget.ImageButton;
import android.widget.LinearLayout;

/**
 * A base implementations of the browser toolbar for tablets.
 * This class manages any Views, variables, etc. that are exclusive to tablet.
 */
abstract class BrowserToolbarTabletBase extends BrowserToolbar {

    protected enum ForwardButtonAnimation {
        SHOW,
        HIDE
    }

    protected final LinearLayout actionItemBar;

    protected final BackButton backButton;
    private final OnClickListener backButtonOnClickListener;
    private final OnLongClickListener backButtonOnLongClickListener;

    protected final ForwardButton forwardButton;
    private final OnClickListener forwardButtonOnClickListener;
    private final OnLongClickListener forwardButtonOnLongClickListener;

    private final Interpolator buttonsInterpolator = new AccelerateInterpolator();

    protected abstract void animateForwardButton(ForwardButtonAnimation animation);

    public BrowserToolbarTabletBase(final Context context, final AttributeSet attrs) {
        super(context, attrs);

        actionItemBar = (LinearLayout) findViewById(R.id.menu_items);

        backButton = (BackButton) findViewById(R.id.back);
        setButtonEnabled(backButton, false);
        forwardButton = (ForwardButton) findViewById(R.id.forward);
        setButtonEnabled(forwardButton, false);

        backButtonOnClickListener = new BackButtonOnClickListener();
        backButtonOnLongClickListener = new BackButtonOnLongClickListener();
        forwardButtonOnClickListener = new ForwardButtonOnClickListener();
        forwardButtonOnLongClickListener = new ForwardButtonOnLongClickListener();
        setNavigationButtonListeners(true);

        focusOrder.addAll(Arrays.asList(tabsButton, (View) backButton, (View) forwardButton, this));
        focusOrder.addAll(urlDisplayLayout.getFocusOrder());
        focusOrder.addAll(Arrays.asList(actionItemBar, menuButton));
    }

    /**
     * Enables or disables the click listeners on the back and forward buttons.
     *
     * This method is useful to remove and later add the listeners when a navigation button is hit
     * because calling `browser.go*()` twice in succession can cause the UI buttons to get out of
     * sync with gecko's browser state (bug 960746).
     *
     * @param disabled True if the listeners should be removed, false for them to be added.
     */
    private void setNavigationButtonListeners(final boolean enabled) {
        if (enabled) {
            backButton.setOnClickListener(backButtonOnClickListener);
            backButton.setOnLongClickListener(backButtonOnLongClickListener);

            forwardButton.setOnClickListener(forwardButtonOnClickListener);
            forwardButton.setOnLongClickListener(forwardButtonOnLongClickListener);
        } else {
            backButton.setOnClickListener(null);
            backButton.setOnLongClickListener(null);

            forwardButton.setOnClickListener(null);
            forwardButton.setOnLongClickListener(null);
        }
    }

    @Override
    public void onTabChanged(final Tab tab, final Tabs.TabEvents msg, final Object data) {
        // STOP appears to be the first page load event where async nav issues are prevented,
        // SELECTED is for switching tabs, and LOAD_ERROR is called when a JavaScript exception
        // is thrown while loading a URI, which can prevent STOP from ever being called.
        //
        // See `setNavigationButtonListeners` javadoc for more information.
        if (msg == TabEvents.STOP ||
                msg == TabEvents.SELECTED ||
                msg == TabEvents.LOAD_ERROR) {
            setNavigationButtonListeners(true);
        }

        super.onTabChanged(tab, msg, data);
    }

    @Override
    protected boolean isTabsButtonOffscreen() {
        return false;
    }

    @Override
    public boolean addActionItem(final View actionItem) {
        actionItemBar.addView(actionItem);
        return true;
    }

    @Override
    public void removeActionItem(final View actionItem) {
        actionItemBar.removeView(actionItem);
    }

    @Override
    protected void updateNavigationButtons(final Tab tab) {
        setButtonEnabled(backButton, canDoBack(tab));

        final boolean isForwardEnabled = canDoForward(tab);
        if (forwardButton.isEnabled() != isForwardEnabled) {
            // Save the state on the forward button so that we can skip animations
            // when there's nothing to change
            setButtonEnabled(forwardButton, isForwardEnabled);
            animateForwardButton(
                    isForwardEnabled ? ForwardButtonAnimation.SHOW : ForwardButtonAnimation.HIDE);
        }
    }

    @Override
    public void setNextFocusDownId(int nextId) {
        super.setNextFocusDownId(nextId);
        backButton.setNextFocusDownId(nextId);
        forwardButton.setNextFocusDownId(nextId);
    }

    @Override
    public void setPrivateMode(final boolean isPrivate) {
        super.setPrivateMode(isPrivate);
        backButton.setPrivateMode(isPrivate);
        forwardButton.setPrivateMode(isPrivate);
    }

    @Override
    public void triggerTabsPanelTransition(final PropertyAnimator animator, final boolean areTabsShown) {
        if (areTabsShown) {
            ViewHelper.setAlpha(tabsCounter, 0.0f);
            return;
        }

        final PropertyAnimator buttonsAnimator =
                new PropertyAnimator(animator.getDuration(), buttonsInterpolator);

        buttonsAnimator.attach(tabsCounter,
                               PropertyAnimator.Property.ALPHA,
                               1.0f);

        buttonsAnimator.start();
    }

    protected boolean canDoBack(final Tab tab) {
        return (tab.canDoBack() && !isEditing());
    }

    protected boolean canDoForward(final Tab tab) {
        return (tab.canDoForward() && !isEditing());
    }

    protected static void setButtonEnabled(final ImageButton button, final boolean enabled) {
        final Drawable drawable = button.getDrawable();
        if (drawable != null) {
            drawable.setAlpha(enabled ? 255 : 61);
        }

        button.setEnabled(enabled);
    }

    private class BackButtonOnClickListener implements OnClickListener {
        @Override
        public void onClick(final View view) {
            setNavigationButtonListeners(false);
            Tabs.getInstance().getSelectedTab().doBack();
        }
    }

    private class BackButtonOnLongClickListener implements OnLongClickListener {
        @Override
        public boolean onLongClick(final View view) {
            setNavigationButtonListeners(false);
            return Tabs.getInstance().getSelectedTab().showBackHistory();
        }
    }

    private class ForwardButtonOnClickListener implements OnClickListener {
        @Override
        public void onClick(final View view) {
            setNavigationButtonListeners(false);
            Tabs.getInstance().getSelectedTab().doForward();
        }
    }

    private class ForwardButtonOnLongClickListener implements OnLongClickListener {
        @Override
        public boolean onLongClick(final View view) {
            setNavigationButtonListeners(false);
            return Tabs.getInstance().getSelectedTab().showForwardHistory();
        }
    }
}
