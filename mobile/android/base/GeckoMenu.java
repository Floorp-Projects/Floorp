/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.SubMenu;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.LinearLayout;
import android.widget.ListView;

import java.util.ArrayList;
import java.util.List;

public class GeckoMenu extends ListView 
                       implements Menu,
                                  MenuItem.OnMenuItemClickListener,
                                  AdapterView.OnItemClickListener,
                                  GeckoMenuItem.OnVisibilityChangedListener,
                                  GeckoMenuItem.OnShowAsActionChangedListener {
    private static final String LOGTAG = "GeckoMenu";

    private Context mContext;

    /*
     * A callback for a menu item selected event.
     */
    public static interface Callback {
        // Called when a menu item is selected, with the actual menu item as the argument.
        public boolean onMenuItemSelected(MenuItem item);
    }

    /*
     * An interface for a presenter to show the menu.
     * Either an Activity or a View can be a presenter, that can watch for events
     * and show/hide menu.
     */
    public static interface MenuPresenter {
        // Open the menu.
        public void openMenu();

        // Show the actual view contaning the menu items. This can either be a parent or sub-menu.
        public void showMenu(View menu);

        // Close the menu.
        public void closeMenu();
    }

    /*
     * An interface for a presenter of action-items.
     * Either an Activity or a View can be a presenter, that can watch for events
     * and add/remove action-items. If not ActionItemBarPresenter, the menu uses a 
     * DefaultActionItemBarPresenter, that shows the action-items as a header over list-view.
     */
    public static interface ActionItemBarPresenter {
        // Add an action-item.
        public void addActionItem(View actionItem);

        // Remove an action-item based on the index. The index used while adding.
        public void removeActionItem(int index);

        // Get the number of action-items shown by the presenter.
        public int getActionItemsCount();
    }

    protected static final int NO_ID = 0;

    // List of all menu items.
    private List<GeckoMenuItem> mItems;

    // List of items in action-bar.
    private List<GeckoMenuItem> mActionItems;

    // Reference to a callback for menu events.
    private Callback mCallback;

    // Reference to menu presenter.
    private MenuPresenter mMenuPresenter;

    // Reference to action-items bar in action-bar.
    private ActionItemBarPresenter mActionItemBarPresenter;

    // Adapter to hold the list of menu items.
    private MenuItemsAdapter mAdapter;

    // ActionBar to show the menu items as icons.
    private LinearLayout mActionBar;

    public GeckoMenu(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;

        setLayoutParams(new LayoutParams(LayoutParams.FILL_PARENT,
                                         LayoutParams.FILL_PARENT));

        // Add a header view that acts as an action-bar.
        mActionBar = (LinearLayout) LayoutInflater.from(mContext).inflate(R.layout.menu_action_bar, null);

        // Attach an adapter.
        mAdapter = new MenuItemsAdapter(context);
        setAdapter(mAdapter);
        setOnItemClickListener(this);

        mItems = new ArrayList<GeckoMenuItem>();
        mActionItems = new ArrayList<GeckoMenuItem>();

        mActionItemBarPresenter = new DefaultActionItemBarPresenter(mContext, mActionBar);
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
        menuItem.setMenu(this);
        menuItem.setOnShowAsActionChangedListener(this);
        menuItem.setOnVisibilityChangedListener(this);
        menuItem.setOnMenuItemClickListener(this);
        mAdapter.addMenuItem(menuItem);
        mItems.add(menuItem);
    }

    private void addActionItem(GeckoMenuItem menuItem) {
        menuItem.setMenu(this);
        menuItem.setOnShowAsActionChangedListener(this);
        menuItem.setOnVisibilityChangedListener(null);
        menuItem.setOnMenuItemClickListener(this);

        if (mActionItems.size() == 0 && 
            mActionItemBarPresenter instanceof DefaultActionItemBarPresenter) {
            // Reset the adapter before adding the header view to a list.
            setAdapter(null);
            addHeaderView(mActionBar);
            setAdapter(mAdapter);
        }

        mActionItems.add(menuItem);
        mActionItemBarPresenter.addActionItem(menuItem.getLayout());
        mItems.add(menuItem);
    }

    @Override
    public int addIntentOptions(int groupId, int itemId, int order, ComponentName caller, Intent[] specifics, Intent intent, int flags, MenuItem[] outSpecificItems) {
        return 0;
    }

    @Override
    public SubMenu addSubMenu(int groupId, int itemId, int order, CharSequence title) {
        MenuItem menuItem = add(groupId, itemId, order, title);
        GeckoSubMenu subMenu = new GeckoSubMenu(mContext, null);
        subMenu.setMenuItem(menuItem);
        subMenu.setCallback(mCallback);
        subMenu.setMenuPresenter(mMenuPresenter);
        ((GeckoMenuItem) menuItem).setSubMenu(subMenu);
        return subMenu;
    }

    @Override
    public SubMenu addSubMenu(int groupId, int itemId, int order, int titleRes) {
        MenuItem menuItem = add(groupId, itemId, order, titleRes);
        GeckoSubMenu subMenu = new GeckoSubMenu(mContext, null);
        subMenu.setMenuItem(menuItem);
        subMenu.setCallback(mCallback);
        subMenu.setMenuPresenter(mMenuPresenter);
        ((GeckoMenuItem) menuItem).setSubMenu(subMenu);
        return subMenu;
    }

    @Override
    public SubMenu addSubMenu(CharSequence title) {
        MenuItem menuItem = add(title);
        GeckoSubMenu subMenu = new GeckoSubMenu(mContext, null);
        subMenu.setMenuItem(menuItem);
        subMenu.setCallback(mCallback);
        subMenu.setMenuPresenter(mMenuPresenter);
        ((GeckoMenuItem) menuItem).setSubMenu(subMenu);
        return subMenu;
    }

    @Override
    public SubMenu addSubMenu(int titleRes) {
        MenuItem menuItem = add(titleRes);
        GeckoSubMenu subMenu = new GeckoSubMenu(mContext, null);
        subMenu.setMenuItem(menuItem);
        subMenu.setCallback(mCallback);
        subMenu.setMenuPresenter(mMenuPresenter);
        ((GeckoMenuItem) menuItem).setSubMenu(subMenu);
        return subMenu;
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
            if (menuItem.getItemId() == id) {
                return menuItem;
            } else if (menuItem.hasSubMenu()) {
                SubMenu subMenu = menuItem.getSubMenu();
                MenuItem item = subMenu.findItem(id);
                if (item != null)
                    return item;
            }
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
        GeckoMenuItem item = (GeckoMenuItem) findItem(id);
        if (item != null) {
            if (mActionItems.contains(item)) {
                if (mActionItemBarPresenter != null)
                    mActionItemBarPresenter.removeActionItem(mActionItems.indexOf(item));

                mActionItems.remove(item);
                mItems.remove(item);

                if (mActionItems.size() == 0 && 
                    mActionItemBarPresenter instanceof DefaultActionItemBarPresenter) {
                    // Reset the adapter before removing the header view from a list.
                    setAdapter(null);
                    removeHeaderView(mActionBar);
                    setAdapter(mAdapter);
                }

                return;
            }

            mAdapter.removeMenuItem(item);
            mItems.remove(item);
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

    @Override
    public void onVisibilityChanged(GeckoMenuItem item, boolean isVisible) {
        if (isVisible)
            mAdapter.addMenuItem(item);
        else
            mAdapter.removeMenuItem(item);
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        position -= getHeaderViewsCount();
        GeckoMenuItem item = mAdapter.getItem(position);
        if (item.isEnabled())
            item.onClick(item.getLayout());
    }

    @Override
    public boolean onMenuItemClick(MenuItem item) {
        if (!item.hasSubMenu()) {
            if (mMenuPresenter != null) 
                mMenuPresenter.closeMenu();

            return mCallback.onMenuItemSelected(item);
        } else {
            // Show the submenu.
            if (mMenuPresenter != null)
                mMenuPresenter.showMenu((GeckoSubMenu) item.getSubMenu());

            return true;
        }
    }

    public boolean onCustomMenuItemClick(MenuItem item, MenuItem.OnMenuItemClickListener listener) {
        if (mMenuPresenter != null)
            mMenuPresenter.closeMenu();

        return listener.onMenuItemClick(item);
    }

    public Callback getCallback() {
        return mCallback;
    }

    public MenuPresenter getMenuPresenter() {
        return mMenuPresenter;
    }

    public void setCallback(Callback callback) {
        mCallback = callback;

        // Update the submenus just in case this changes on the fly.
        for (GeckoMenuItem menuItem : mItems) {
            if (menuItem.hasSubMenu()) {
                GeckoSubMenu subMenu = (GeckoSubMenu) menuItem.getSubMenu();
                subMenu.setCallback(mCallback);
            }
        }
    }

    public void setMenuPresenter(MenuPresenter presenter) {
        mMenuPresenter = presenter;

        // Update the submenus just in case this changes on the fly.
        for (GeckoMenuItem menuItem : mItems) {
            if (menuItem.hasSubMenu()) {
                GeckoSubMenu subMenu = (GeckoSubMenu) menuItem.getSubMenu();
                subMenu.setMenuPresenter(mMenuPresenter);
            }
        }
    }

    public void setActionItemBarPresenter(ActionItemBarPresenter presenter) {
        mActionItemBarPresenter = presenter;
    }

    // Action Items are added to the header view by default.
    // URL bar can register itself as a presenter, in case it has a different place to show them.
    private class DefaultActionItemBarPresenter implements ActionItemBarPresenter {
        private Context mContext;
        private LinearLayout mContainer;
        private List<View> mItems;
 
        public DefaultActionItemBarPresenter(Context context, LinearLayout container) {
            mContext = context;
            mContainer = container;
            mItems = new ArrayList<View>();
        }

        @Override
        public void addActionItem(View actionItem) {
            LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(actionItem.getLayoutParams());
            params.weight = 1.0f;
            actionItem.setLayoutParams(params);

            if (mItems.size() > 0) {
                Divider divider = new Divider(mContext, null);
                divider.setOrientation(Divider.Orientation.VERTICAL);
                divider.setBackgroundColor(0xFFD1D5DA);
                mContainer.addView(divider);
            }

            mContainer.addView(actionItem);
            mItems.add(actionItem);
        }

        @Override
        public void removeActionItem(int index) {
            // Remove the icon and the vertical divider.
            mContainer.removeViewAt(index * 2);

            if (index != 0)
                mContainer.removeViewAt(index * 2 - 1);

            mItems.remove(index);

            if (mItems.size() == 0)
                mContainer.setVisibility(View.GONE);
        }

        @Override
        public int getActionItemsCount() {
            return mItems.size();
        }
    }

    // Adapter to bind menu items to the list.
    private class MenuItemsAdapter extends BaseAdapter {
        private Context mContext;
        private List<GeckoMenuItem> mItems;

        public MenuItemsAdapter(Context context) {
            mContext = context;
            mItems = new ArrayList<GeckoMenuItem>();
        }

        @Override
        public int getCount() {
            return (mItems == null ? 0 : mItems.size());
        }

        @Override
        public GeckoMenuItem getItem(int position) {
            return mItems.get(position);
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            return mItems.get(position).getLayout();
        }

        @Override
        public int getItemViewType (int position) {
            return AdapterView.ITEM_VIEW_TYPE_IGNORE;
        }

        @Override
        public boolean areAllItemsEnabled() {
            for (GeckoMenuItem item : mItems) {
                 if (!item.isEnabled())
                     return false;
            }

            return true;
        }

        @Override
        public boolean isEnabled(int position) {
            return getItem(position).isEnabled();
        }

        public void addMenuItem(GeckoMenuItem menuItem) {
            if (mItems.contains(menuItem))
                return;

            // Insert it in proper order.
            int index = 0;
            for (GeckoMenuItem item : mItems) {
                if (item.getOrder() > menuItem.getOrder()) {
                    mItems.add(index, menuItem);
                    notifyDataSetChanged();
                    return;
                } else {
                  index++;
                }
            }

            // Add the menuItem at the end.
            mItems.add(menuItem);
            notifyDataSetChanged();
        }

        public void removeMenuItem(GeckoMenuItem menuItem) {
            // Remove it from the list.
            mItems.remove(menuItem);
            notifyDataSetChanged();
        }

        public GeckoMenuItem getMenuItem(int id) {
            for (GeckoMenuItem item : mItems) {
                if (item.getItemId() == id)
                    return item;
            }

            return null;
        }
    }
}
