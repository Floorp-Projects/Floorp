/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import java.util.Arrays;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.tabs.TabHistoryController;
import org.mozilla.gecko.menu.MenuItemActionBar;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.Button;
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
    protected final ForwardButton forwardButton;

    protected abstract void animateForwardButton(ForwardButtonAnimation animation);

    public BrowserToolbarTabletBase(final Context context, final AttributeSet attrs) {
        super(context, attrs);

        actionItemBar = (LinearLayout) findViewById(R.id.menu_items);

        backButton = (BackButton) findViewById(R.id.back);
        setButtonEnabled(backButton, false);
        forwardButton = (ForwardButton) findViewById(R.id.forward);
        setButtonEnabled(forwardButton, false);
        initButtonListeners();

        focusOrder.addAll(Arrays.asList(tabsButton, (View) backButton, (View) forwardButton, this));
        focusOrder.addAll(urlDisplayLayout.getFocusOrder());
        focusOrder.addAll(Arrays.asList(actionItemBar, menuButton));
    }

    private void initButtonListeners() {
        backButton.setOnClickListener(new Button.OnClickListener() {
            @Override
            public void onClick(View view) {
                Tabs.getInstance().getSelectedTab().doBack();
            }
        });
        backButton.setOnLongClickListener(new Button.OnLongClickListener() {
            @Override
            public boolean onLongClick(View view) {
                return tabHistoryController.showTabHistory(Tabs.getInstance().getSelectedTab(),
                        TabHistoryController.HistoryAction.BACK);
            }
        });

        forwardButton.setOnClickListener(new Button.OnClickListener() {
            @Override
            public void onClick(View view) {
                Tabs.getInstance().getSelectedTab().doForward();
            }
        });
        forwardButton.setOnLongClickListener(new Button.OnLongClickListener() {
            @Override
            public boolean onLongClick(View view) {
                return tabHistoryController.showTabHistory(Tabs.getInstance().getSelectedTab(),
                        TabHistoryController.HistoryAction.FORWARD);
            }
        });
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
        animateForwardButton(
                canDoForward(tab) ? ForwardButtonAnimation.SHOW : ForwardButtonAnimation.HIDE);
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
        for (int i = 0; i < actionItemBar.getChildCount(); ++i) {
            final MenuItemActionBar child = (MenuItemActionBar) actionItemBar.getChildAt(i);
            child.setPrivateMode(isPrivate);
        }
    }

    @Override
    public View getDoorHangerAnchor() {
        return backButton;
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
}
