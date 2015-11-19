/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.menu;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.R;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.ThreadUtils.AssertBehavior;
import org.mozilla.gecko.widget.GeckoActionProvider;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.util.AttributeSet;
import android.util.Log;
import android.util.SparseArray;
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
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class GeckoMenu extends ListView 
                       implements Menu,
                                  AdapterView.OnItemClickListener,
                                  GeckoMenuItem.OnShowAsActionChangedListener {
    private static final String LOGTAG = "GeckoMenu";

    /**
     * Controls whether off-UI-thread method calls in this class cause an
     * exception or just logging.
     */
    private static final AssertBehavior THREAD_ASSERT_BEHAVIOR = AppConstants.RELEASE_BUILD ? AssertBehavior.NONE : AssertBehavior.THROW;

    /*
     * A callback for a menu item click/long click event.
     */
    public static interface Callback {
        // Called when a menu item is clicked, with the actual menu item as the argument.
        public boolean onMenuItemClick(MenuItem item);

        // Called when a menu item is long-clicked, with the actual menu item as the argument.
        public boolean onMenuItemLongClick(MenuItem item);
    }

    /*
     * An interface for a presenter to show the menu.
     * Either an Activity or a View can be a presenter, that can watch for events
     * and show/hide menu.
     */
    public static interface MenuPresenter {
        // Open the menu.
        public void openMenu();

        // Show the actual view containing the menu items. This can either be a parent or sub-menu.
        public void showMenu(View menu);

        // Close the menu.
        public void closeMenu();
    }

    /*
     * An interface for a presenter of action-items.
     * Either an Activity or a View can be a presenter, that can watch for events
     * and add/remove action-items. If not ActionItemBarPresenter, the menu uses a 
     * DefaultActionItemBar, that shows the action-items as a header over list-view.
     */
    public static interface ActionItemBarPresenter {
        // Add an action-item.
        public boolean addActionItem(View actionItem);

        // Remove an action-item.
        public void removeActionItem(View actionItem);
    }

    protected static final int NO_ID = 0;

    // List of all menu items.
    private final List<GeckoMenuItem> mItems;

    // Quick lookup array used to make a fast path in findItem.
    private final SparseArray<MenuItem> mItemsById;

    // Map of "always" action-items in action-bar and their views.
    private final Map<GeckoMenuItem, View> mPrimaryActionItems;

    // Map of "ifRoom" action-items in action-bar and their views.
    private final Map<GeckoMenuItem, View> mSecondaryActionItems;

    // Map of "ifRoom|withText" action-items in action-bar and their views.
    private final Map<GeckoMenuItem, View> mShareActionItems;

    // Reference to a callback for menu events.
    private Callback mCallback;

    // Reference to menu presenter.
    private MenuPresenter mMenuPresenter;

    // Reference to "always" action-items bar in action-bar.
    private ActionItemBarPresenter mPrimaryActionItemBar;

    // Reference to "ifRoom" action-items bar in action-bar.
    private final ActionItemBarPresenter mSecondaryActionItemBar;

    // Reference to "ifRoom|withText" action-items bar in action-bar.
    private final ActionItemBarPresenter mShareActionItemBar;

    // Adapter to hold the list of menu items.
    private final MenuItemsAdapter mAdapter;

    // Show/hide icons in the list.
    boolean mShowIcons;

    public GeckoMenu(Context context) {
        this(context, null);
    }

    public GeckoMenu(Context context, AttributeSet attrs) {
        this(context, attrs, R.attr.geckoMenuListViewStyle);
    }

    public GeckoMenu(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);

        setLayoutParams(new LayoutParams(LayoutParams.MATCH_PARENT,
                                         LayoutParams.MATCH_PARENT));

        // Attach an adapter.
        mAdapter = new MenuItemsAdapter();
        setAdapter(mAdapter);
        setOnItemClickListener(this);

        mItems = new ArrayList<GeckoMenuItem>();
        mItemsById = new SparseArray<MenuItem>();
        mPrimaryActionItems = new HashMap<GeckoMenuItem, View>();
        mSecondaryActionItems = new HashMap<GeckoMenuItem, View>();
        mShareActionItems = new HashMap<GeckoMenuItem, View>();

        mPrimaryActionItemBar = (DefaultActionItemBar) LayoutInflater.from(context).inflate(R.layout.menu_action_bar, null);
        mSecondaryActionItemBar = (DefaultActionItemBar) LayoutInflater.from(context).inflate(R.layout.menu_secondary_action_bar, null);
        mShareActionItemBar = (DefaultActionItemBar) LayoutInflater.from(context).inflate(R.layout.menu_secondary_action_bar, null);
    }

    private static void assertOnUiThread() {
        ThreadUtils.assertOnUiThread(THREAD_ASSERT_BEHAVIOR);
    }

    @Override
    public MenuItem add(CharSequence title) {
        GeckoMenuItem menuItem = new GeckoMenuItem(this, NO_ID, 0, title);
        addItem(menuItem);
        return menuItem;
    }

    @Override
    public MenuItem add(int groupId, int itemId, int order, int titleRes) {
        GeckoMenuItem menuItem = new GeckoMenuItem(this, itemId, order, titleRes);
        addItem(menuItem);
        return menuItem;
    }

    @Override
    public MenuItem add(int titleRes) {
        GeckoMenuItem menuItem = new GeckoMenuItem(this, NO_ID, 0, titleRes);
        addItem(menuItem);
        return menuItem;
    }

    @Override
    public MenuItem add(int groupId, int itemId, int order, CharSequence title) {
        GeckoMenuItem menuItem = new GeckoMenuItem(this, itemId, order, title);
        addItem(menuItem);
        return menuItem;
    }

    private void addItem(GeckoMenuItem menuItem) {
        assertOnUiThread();
        menuItem.setOnShowAsActionChangedListener(this);
        mAdapter.addMenuItem(menuItem);
        mItems.add(menuItem);
    }

    private boolean addActionItem(final GeckoMenuItem menuItem) {
        assertOnUiThread();
        menuItem.setOnShowAsActionChangedListener(this);

        final View actionView = menuItem.getActionView();
        final int actionEnum = menuItem.getActionEnum();
        boolean added = false;

        if (actionEnum == GeckoMenuItem.SHOW_AS_ACTION_ALWAYS) {
            if (mPrimaryActionItems.size() == 0 &&
                mPrimaryActionItemBar instanceof DefaultActionItemBar) {
                // Reset the adapter before adding the header view to a list.
                setAdapter(null);
                addHeaderView((DefaultActionItemBar) mPrimaryActionItemBar);
                setAdapter(mAdapter);
            }

            if (added = mPrimaryActionItemBar.addActionItem(actionView)) {
                mPrimaryActionItems.put(menuItem, actionView);
                mItems.add(menuItem);
            }
        } else if (actionEnum == GeckoMenuItem.SHOW_AS_ACTION_IF_ROOM) {
            if (mSecondaryActionItems.size() == 0) {
                // Reset the adapter before adding the header view to a list.
                setAdapter(null);
                addHeaderView((DefaultActionItemBar) mSecondaryActionItemBar);
                setAdapter(mAdapter);
            }

            if (added = mSecondaryActionItemBar.addActionItem(actionView)) {
                mSecondaryActionItems.put(menuItem, actionView);
                mItems.add(menuItem);
            }
        } else if (actionEnum == GeckoMenuItem.SHOW_AS_ACTION_IF_ROOM_WITH_TEXT) {
            if (added = mShareActionItemBar.addActionItem(actionView)) {
                if (mShareActionItems.size() == 0) {
                    // Reset the adapter before adding the header view to a list.
                    setAdapter(null);
                    addHeaderView((DefaultActionItemBar) mShareActionItemBar);
                    setAdapter(mAdapter);
                }

                mShareActionItems.put(menuItem, actionView);
                mItems.add(menuItem);
            }
        }

        // Set the listeners.
        if (actionView instanceof MenuItemActionBar) {
            ((MenuItemActionBar) actionView).setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View view) {
                    handleMenuItemClick(menuItem);
                }
            });
            ((MenuItemActionBar) actionView).setOnLongClickListener(new View.OnLongClickListener() {
                @Override
                public boolean onLongClick(View view) {
                    if (handleMenuItemLongClick(menuItem)) {
                        GeckoAppShell.vibrateOnHapticFeedbackEnabled(getResources().getIntArray(R.array.long_press_vibrate_msec));
                        return true;
                    }
                    return false;
                }
            });
        } else if (actionView instanceof MenuItemActionView) {
            ((MenuItemActionView) actionView).setMenuItemClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View view) {
                    handleMenuItemClick(menuItem);
                }
            });
            ((MenuItemActionView) actionView).setMenuItemLongClickListener(new View.OnLongClickListener() {
                @Override
                public boolean onLongClick(View view) {
                    if (handleMenuItemLongClick(menuItem)) {
                        GeckoAppShell.vibrateOnHapticFeedbackEnabled(getResources().getIntArray(R.array.long_press_vibrate_msec));
                        return true;
                    }
                    return false;
                }
            });
        }

        return added;
    }

    @Override
    public int addIntentOptions(int groupId, int itemId, int order, ComponentName caller, Intent[] specifics, Intent intent, int flags, MenuItem[] outSpecificItems) {
        return 0;
    }

    @Override
    public SubMenu addSubMenu(int groupId, int itemId, int order, CharSequence title) {
        MenuItem menuItem = add(groupId, itemId, order, title);
        return addSubMenu(menuItem);
    }

    @Override
    public SubMenu addSubMenu(int groupId, int itemId, int order, int titleRes) {
        MenuItem menuItem = add(groupId, itemId, order, titleRes);
        return addSubMenu(menuItem);
    }

    @Override
    public SubMenu addSubMenu(CharSequence title) {
        MenuItem menuItem = add(title);
        return addSubMenu(menuItem);
    }

    @Override
    public SubMenu addSubMenu(int titleRes) {
        MenuItem menuItem = add(titleRes);
        return addSubMenu(menuItem);
    }

    private SubMenu addSubMenu(MenuItem menuItem) {
        GeckoSubMenu subMenu = new GeckoSubMenu(getContext());
        subMenu.setMenuItem(menuItem);
        subMenu.setCallback(mCallback);
        subMenu.setMenuPresenter(mMenuPresenter);
        ((GeckoMenuItem) menuItem).setSubMenu(subMenu);
        return subMenu;
    }

    private void removePrimaryActionBarView() {
        // Reset the adapter before removing the header view from a list.
        setAdapter(null);
        removeHeaderView((DefaultActionItemBar) mPrimaryActionItemBar);
        setAdapter(mAdapter);
    }

    private void removeSecondaryActionBarView() {
        // Reset the adapter before removing the header view from a list.
        setAdapter(null);
        removeHeaderView((DefaultActionItemBar) mSecondaryActionItemBar);
        setAdapter(mAdapter);
    }

    private void removeShareActionBarView() {
        // Reset the adapter before removing the header view from a list.
        setAdapter(null);
        removeHeaderView((DefaultActionItemBar) mShareActionItemBar);
        setAdapter(mAdapter);
    }

    @Override
    public void clear() {
        assertOnUiThread();
        for (GeckoMenuItem menuItem : mItems) {
            if (menuItem.hasSubMenu()) {
                SubMenu sub = menuItem.getSubMenu();
                if (sub == null) {
                    continue;
                }
                try {
                    sub.clear();
                } catch (Exception ex) {
                    Log.e(LOGTAG, "Couldn't clear submenu.", ex);
                }
            }
        }

        mAdapter.clear();
        mItems.clear();

        /*
         * Reinflating the menu will re-add any action items to the toolbar, so
         * remove the old ones. This also ensures that any text associated with
         * these is switched to the correct locale.
         */
        if (mPrimaryActionItemBar != null) {
            for (View item : mPrimaryActionItems.values()) {
                mPrimaryActionItemBar.removeActionItem(item);
            }
        }
        mPrimaryActionItems.clear();

        if (mSecondaryActionItemBar != null) {
            for (View item : mSecondaryActionItems.values()) {
                mSecondaryActionItemBar.removeActionItem(item);
            }
        }
        mSecondaryActionItems.clear();

        if (mShareActionItemBar != null) {
            for (View item : mShareActionItems.values()) {
                mShareActionItemBar.removeActionItem(item);
            }
        }
        mShareActionItems.clear();

        // Remove the view, too -- the first addActionItem will re-add it,
        // and this is simpler than changing that logic.
        if (mPrimaryActionItemBar instanceof DefaultActionItemBar) {
            removePrimaryActionBarView();
        }

        removeSecondaryActionBarView();
        removeShareActionBarView();
    }

    @Override
    public void close() {
        if (mMenuPresenter != null)
            mMenuPresenter.closeMenu();
    }

    private void showMenu(View viewForMenu) {
        if (mMenuPresenter != null)
            mMenuPresenter.showMenu(viewForMenu);
    }

    @Override
    public MenuItem findItem(int id) {
        assertOnUiThread();
        MenuItem quickItem = mItemsById.get(id);
        if (quickItem != null) {
            return quickItem;
        }

        for (GeckoMenuItem menuItem : mItems) {
            if (menuItem.getItemId() == id) {
                mItemsById.put(id, menuItem);
                return menuItem;
            } else if (menuItem.hasSubMenu()) {
                if (!menuItem.hasActionProvider()) {
                    SubMenu subMenu = menuItem.getSubMenu();
                    MenuItem item = subMenu.findItem(id);
                    if (item != null) {
                        mItemsById.put(id, item);
                        return item;
                    }
                }
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
        assertOnUiThread();
        for (GeckoMenuItem menuItem : mItems) {
            if (menuItem.isVisible() &&
                !mPrimaryActionItems.containsKey(menuItem) &&
                !mSecondaryActionItems.containsKey(menuItem) &&
                !mShareActionItems.containsKey(menuItem))
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
        assertOnUiThread();
        GeckoMenuItem item = (GeckoMenuItem) findItem(id);
        if (item == null)
            return;

        // Remove it from the cache.
        mItemsById.remove(id);

        // Remove it from any sub-menu.
        for (GeckoMenuItem menuItem : mItems) {
            if (menuItem.hasSubMenu()) {
                SubMenu subMenu = menuItem.getSubMenu();
                if (subMenu != null && subMenu.findItem(id) != null) {
                    subMenu.removeItem(id);
                    return;
                }
            }
        }

        // Remove it from own menu.
        if (mPrimaryActionItems.containsKey(item)) {
            if (mPrimaryActionItemBar != null)
                mPrimaryActionItemBar.removeActionItem(mPrimaryActionItems.get(item));

            mPrimaryActionItems.remove(item);
            mItems.remove(item);

            if (mPrimaryActionItems.size() == 0 && 
                mPrimaryActionItemBar instanceof DefaultActionItemBar) {
                removePrimaryActionBarView();
            }

            return;
        }

        if (mSecondaryActionItems.containsKey(item)) {
            if (mSecondaryActionItemBar != null)
                mSecondaryActionItemBar.removeActionItem(mSecondaryActionItems.get(item));

            mSecondaryActionItems.remove(item);
            mItems.remove(item);

            if (mSecondaryActionItems.size() == 0) {
                removeSecondaryActionBarView();
            }

            return;
        }

        if (mShareActionItems.containsKey(item)) {
            if (mShareActionItemBar != null)
                mShareActionItemBar.removeActionItem(mShareActionItems.get(item));

            mShareActionItems.remove(item);
            mItems.remove(item);

            if (mShareActionItems.size() == 0) {
                removeShareActionBarView();
            }

            return;
        }

        mAdapter.removeMenuItem(item);
        mItems.remove(item);
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
         return (mPrimaryActionItemBar != null) &&
                 (mSecondaryActionItemBar != null) &&
                 (mShareActionItemBar != null);
    }

    @Override
    public void onShowAsActionChanged(GeckoMenuItem item) {
        removeItem(item.getItemId());

        if (item.isActionItem() && addActionItem(item)) {
            return;
        }

        addItem(item);
    }

    public void onItemChanged(GeckoMenuItem item) {
        assertOnUiThread();
        if (item.isActionItem()) {
            final View actionView;
            final int actionEnum = item.getActionEnum();
            if (actionEnum == GeckoMenuItem.SHOW_AS_ACTION_ALWAYS) {
                actionView = mPrimaryActionItems.get(item);
            } else if (actionEnum == GeckoMenuItem.SHOW_AS_ACTION_IF_ROOM) {
                actionView = mSecondaryActionItems.get(item);
            } else {
                actionView = mShareActionItems.get(item);
            }

            if (actionView != null) {
                if (item.isVisible()) {
                    actionView.setVisibility(View.VISIBLE);
                    if (actionView instanceof MenuItemActionBar) {
                        ((MenuItemActionBar) actionView).initialize(item);
                    } else {
                        ((MenuItemActionView) actionView).initialize(item);
                    }
                } else {
                    actionView.setVisibility(View.GONE);
                }
            }
        } else {
            mAdapter.notifyDataSetChanged();
        }
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        // We might be showing headers. Account them while using the position.
        position -= getHeaderViewsCount();

        GeckoMenuItem item = mAdapter.getItem(position);
        handleMenuItemClick(item);
    }

    void handleMenuItemClick(GeckoMenuItem item) {
        if (!item.isEnabled())
            return;

        if (item.invoke()) {
            close();
        } else if (item.hasSubMenu()) {
            // Refresh the submenu for the provider.
            GeckoActionProvider provider = item.getGeckoActionProvider();
            if (provider != null) {
                GeckoSubMenu subMenu = new GeckoSubMenu(getContext());
                subMenu.setShowIcons(true);
                provider.onPrepareSubMenu(subMenu);
                item.setSubMenu(subMenu);
            }

            // Show the submenu.
            GeckoSubMenu subMenu = (GeckoSubMenu) item.getSubMenu();
            showMenu(subMenu);
        } else {
            close();
            mCallback.onMenuItemClick(item);
        }
    }

    boolean handleMenuItemLongClick(GeckoMenuItem item) {
        if (!item.isEnabled()) {
            return false;
        }

        if (mCallback != null) {
            if (mCallback.onMenuItemLongClick(item)) {
                close();
                return true;
            }
        }
        return false;
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
        mPrimaryActionItemBar = presenter;
    }

    public void setShowIcons(boolean show) {
        if (mShowIcons != show) {
            mShowIcons = show;
            mAdapter.notifyDataSetChanged();
        }
    }

    // Action Items are added to the header view by default.
    // URL bar can register itself as a presenter, in case it has a different place to show them.
    public static class DefaultActionItemBar extends LinearLayout
                                             implements ActionItemBarPresenter {
        private final int mRowHeight;
        private float mWeightSum;

        public DefaultActionItemBar(Context context) {
            this(context, null);
        }

        public DefaultActionItemBar(Context context, AttributeSet attrs) {
            super(context, attrs);

            mRowHeight = getResources().getDimensionPixelSize(R.dimen.menu_item_row_height);
        }

        @Override
        public boolean addActionItem(View actionItem) {
            ViewGroup.LayoutParams actualParams = actionItem.getLayoutParams();
            LinearLayout.LayoutParams params;

            if (actualParams != null) {
                params = new LinearLayout.LayoutParams(actionItem.getLayoutParams());
                params.width = 0;
            } else {
                params = new LinearLayout.LayoutParams(0, mRowHeight);
            }

            if (actionItem instanceof MenuItemActionView) {
                params.weight = ((MenuItemActionView) actionItem).getChildCount();
            } else {
                params.weight = 1.0f;
            }

            mWeightSum += params.weight;

            actionItem.setLayoutParams(params);
            addView(actionItem);
            setWeightSum(mWeightSum);
            return true;
        }

        @Override
        public void removeActionItem(View actionItem) {
            if (indexOfChild(actionItem) != -1) {
                LinearLayout.LayoutParams params = (LinearLayout.LayoutParams) actionItem.getLayoutParams();
                mWeightSum -= params.weight;
                removeView(actionItem);
            }
        }
    }

    // Adapter to bind menu items to the list.
    private class MenuItemsAdapter extends BaseAdapter {
        private static final int VIEW_TYPE_DEFAULT = 0;
        private static final int VIEW_TYPE_ACTION_MODE = 1;

        private final List<GeckoMenuItem> mItems;

        public MenuItemsAdapter() {
            mItems = new ArrayList<GeckoMenuItem>();
        }

        @Override
        public int getCount() {
            if (mItems == null)
                return 0;

            int visibleCount = 0;
            for (GeckoMenuItem item : mItems) {
                if (item.isVisible())
                    visibleCount++;
            }

            return visibleCount;
        }

        @Override
        public GeckoMenuItem getItem(int position) {
            for (GeckoMenuItem item : mItems) {
                if (item.isVisible()) {
                    position--;

                    if (position < 0)
                        return item;
                }
            }

            return null;
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            GeckoMenuItem item = getItem(position);
            GeckoMenuItem.Layout view = null;

            // Try to re-use the view.
            if (convertView == null && getItemViewType(position) == VIEW_TYPE_DEFAULT) {
                view = new MenuItemDefault(parent.getContext(), null);
            } else {
                view = (GeckoMenuItem.Layout) convertView;
            }

            if (view == null || view instanceof MenuItemActionView) {
                // Always get from the menu item.
                // This will ensure that the default activity is refreshed.
                view = (MenuItemActionView) item.getActionView();

                // ListView will not perform an item click if the row has a focusable view in it.
                // Hence, forward the click event on the menu item in the action-view to the ListView.
                final View actionView = (View) view;
                final int pos = position;
                final long id = getItemId(position);
                ((MenuItemActionView) view).setMenuItemClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        GeckoMenu listView = GeckoMenu.this;
                        listView.performItemClick(actionView, pos + listView.getHeaderViewsCount(), id);
                    }
                });
            }

            // Initialize the view.
            view.setShowIcon(mShowIcons);
            view.initialize(item);
            return (View) view; 
        }

        @Override
        public int getItemViewType(int position) {
            return getItem(position).getGeckoActionProvider() == null ? VIEW_TYPE_DEFAULT : VIEW_TYPE_ACTION_MODE;
        }

        @Override
        public int getViewTypeCount() {
            return 2;
        }

        @Override
        public boolean hasStableIds() {
            return false;
        }

        @Override
        public boolean areAllItemsEnabled() {
            // Setting this to true is a workaround to fix disappearing
            // dividers in the menu (bug 963249).
            return true;
        }

        @Override
        public boolean isEnabled(int position) {
            // Setting this to true is a workaround to fix disappearing
            // dividers in the menu in L (bug 1050780).
            return true;
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

        public void clear() {
            mItemsById.clear();
            mItems.clear();
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
