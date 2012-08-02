/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.SubMenu;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;

import java.util.ArrayList;
import java.util.List;

public class GeckoMenu extends LinearLayout 
                       implements Menu, GeckoMenuItem.OnShowAsActionChangedListener {
    private static final String LOGTAG = "GeckoMenu";

    private Context mContext;

    public static interface ActionItemBarPresenter {
        public void addActionItem(View actionItem);
        public void removeActionItem(int index);
        public int getActionItemsCount();
    }

    private static final int NO_ID = 0;

    // Default list of items.
    private List<GeckoMenuItem> mItems;

    // List of items in action-bar.
    private List<GeckoMenuItem> mActionItems;

    // Reference to action-items bar in action-bar.
    private ActionItemBarPresenter mActionItemBarPresenter;

    public GeckoMenu(Context context, AttributeSet attrs) {
        super(context, attrs);

        mContext = context;

        setLayoutParams(new LayoutParams(LayoutParams.FILL_PARENT,
                                         LayoutParams.WRAP_CONTENT));
        setOrientation(VERTICAL);

        mItems = new ArrayList<GeckoMenuItem>();
        mActionItems = new ArrayList<GeckoMenuItem>();
    }

    @Override
    public MenuItem add(CharSequence title) {
        GeckoMenuItem menuItem = new GeckoMenuItem(mContext, NO_ID);
        menuItem.setTitle(title);
        addItem(menuItem);
        return menuItem;
    }

    @Override
    public MenuItem add(int groupId, int itemId, int order, int titleRes) {
        GeckoMenuItem menuItem = new GeckoMenuItem(mContext, itemId, order);
        menuItem.setTitle(titleRes);
        addItem(menuItem);
        return menuItem;
    }

    @Override
    public MenuItem add(int titleRes) {
        GeckoMenuItem menuItem = new GeckoMenuItem(mContext, NO_ID);
        menuItem.setTitle(titleRes);
        addItem(menuItem);
        return menuItem;
    }

    @Override
    public MenuItem add(int groupId, int itemId, int order, CharSequence title) {
        GeckoMenuItem menuItem = new GeckoMenuItem(mContext, itemId, order);
        menuItem.setTitle(title);
        addItem(menuItem);
        return menuItem;
    }

    private void addItem(GeckoMenuItem menuItem) {
        menuItem.setOnShowAsActionChangedListener(this);

        // Insert it in proper order.
        int index = 0;
        for (GeckoMenuItem item : mItems) {
            if (item.getOrder() > menuItem.getOrder()) {
                mItems.add(index, menuItem);

                // Account for the items in the action-bar.
                if (mActionItemBarPresenter != null)
                    addView(menuItem.getLayout(), index - mActionItemBarPresenter.getActionItemsCount());
                else
                    addView(menuItem.getLayout(), index);

                return;
            } else {
              index++;
            }
        }

        // Add the menuItem at the end.
        mItems.add(menuItem);
        addView(menuItem.getLayout());
    }

    private void addActionItem(GeckoMenuItem menuItem) {
        menuItem.setOnShowAsActionChangedListener(this);

        int index = 0;
        for (GeckoMenuItem item : mItems) {
            if (item.getOrder() <= menuItem.getOrder())
                index++;
            else
                break;
        }

        mActionItems.add(menuItem);
        mItems.add(index, menuItem);
        mActionItemBarPresenter.addActionItem(menuItem.getLayout());
    }

    @Override
    public int addIntentOptions(int groupId, int itemId, int order, ComponentName caller, Intent[] specifics, Intent intent, int flags, MenuItem[] outSpecificItems) {
        return 0;
    }

    @Override
    public SubMenu addSubMenu(int groupId, int itemId, int order, CharSequence title) {
        return null;
    }

    @Override
    public SubMenu addSubMenu(int groupId, int itemId, int order, int titleRes) {
        return null;
    }

    @Override
    public SubMenu addSubMenu(CharSequence title) {
        return null;
    }

    @Override
    public SubMenu addSubMenu(int titleRes) {
       return null;
    }

    @Override
    public void clear() {
    }

    @Override
    public void close() {
    }

    @Override
    public MenuItem findItem(int id) {
        for (GeckoMenuItem menuItem : mItems) {
            if (menuItem.getItemId() == id)
                return menuItem;
        }
        return null;
    }

    @Override
    public MenuItem getItem(int index) {
        if (index < mItems.size())
            return mItems.get(index);

        return null;
    }

    @Override
    public boolean hasVisibleItems() {
        for (GeckoMenuItem menuItem : mItems) {
            if (menuItem.isVisible())
                return true;
        }

        return false;
    }

    @Override
    public boolean isShortcutKey(int keyCode, KeyEvent event) {
        return true;
    }

    @Override
    public boolean performIdentifierAction(int id, int flags) {
        return false;
    }

    @Override
    public boolean performShortcut(int keyCode, KeyEvent event, int flags) {
        return false;
    }

    @Override
    public void removeGroup(int groupId) {
    }

    @Override
    public void removeItem(int id) {
        for (GeckoMenuItem menuItem : mItems) {
            if (menuItem.getItemId() == id) {
                if (mActionItems.contains(menuItem)) {
                    if (mActionItemBarPresenter != null)
                        mActionItemBarPresenter.removeActionItem(mActionItems.indexOf(menuItem));

                   mActionItems.remove(menuItem);
                } else {
                    removeView(findViewById(id));
                }

                mItems.remove(menuItem);
                return;
            }
        }
    }

    @Override
    public void setGroupCheckable(int group, boolean checkable, boolean exclusive) {
    }

    @Override
    public void setGroupEnabled(int group, boolean enabled) {
    }

    @Override
    public void setGroupVisible(int group, boolean visible) {
    }

    @Override
    public void setQwertyMode(boolean isQwerty) {
    }

    @Override
    public int size() {
        return mItems.size();
    }

    @Override
    public boolean hasActionItemBar() {
         return (mActionItemBarPresenter != null);
    }

    @Override
    public void onShowAsActionChanged(GeckoMenuItem item, boolean isActionItem) {
        removeItem(item.getItemId());

        if (isActionItem)
            addActionItem(item);
        else
            addItem(item);
    }

    public void setActionItemBarPresenter(ActionItemBarPresenter presenter) {
        mActionItemBarPresenter = presenter;
    }
}
