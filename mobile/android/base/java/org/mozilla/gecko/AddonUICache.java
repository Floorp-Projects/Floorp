/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.VisibleForTesting;
import android.text.TextUtils;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.SubMenu;

import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import static org.mozilla.gecko.toolbar.PageActionLayout.PageAction;
import static org.mozilla.gecko.toolbar.PageActionLayout.PageActionLayoutDelegate;

/**
 * For certain UI items added by add-ons or other JS/Gecko code, Gecko notifies us whenever an item
 * is added, changed or removed. Since we must not miss any of these notifications and need to re-
 * member the current list of active UI items even if e.g. we're in background and our activities
 * have been destroyed, we need a class whose lifetime matches (or even exceeds) that of Gecko.
 *
 * This class fulfills this purpose - it will be initialised early during app startup and just like
 * Gecko, once initialised it will remain alive until the OS kills our app.
 *
 * After initialisation, we will start listening for the appropriate EventDispatcher messages from
 * Gecko and maintain an internal list of UI items dynamically added by Gecko.
 * In addition, for each class of UI elements an appropriate API will be provided through which the
 * intended final consumer can make use of that list in order to actually show those elements in the
 * UI.
 */
public class AddonUICache implements BundleEventListener {
    private static final String LOGTAG = "GeckoAddonUICache";

    private static final int GECKO_TOOLS_MENU_ID = -1;
    // When changing this, make sure to adjust NativeWindow.toolsMenuID in browser.js, too.
    private static final String GECKO_TOOLS_MENU_UUID = "{115b9308-2023-44f1-a4e9-3e2197669f07}";
    private static final int ADDON_MENU_OFFSET = 1000;

    private static class MenuItemInfo {
        public int id;
        public String uuid;
        public String label;
        public boolean checkable;
        public boolean checked;
        public boolean enabled = true;
        public boolean visible = true;
        public int parent;
    }

    private static final AddonUICache instance = new AddonUICache();

    private final Map<String, MenuItemInfo> mAddonMenuItemsCache = new LinkedHashMap<>();
    private int mAddonMenuNextID = ADDON_MENU_OFFSET;
    private Menu mMenu;

    // A collection of PageAction messages that are still pending transformation into
    // a full PageAction - most importantly transformation of the image data URI
    // into a Drawable.
    private final Map<String, GeckoBundle> mPendingPageActionQueue = new LinkedHashMap<>();
    // A collection of PageActions ready for immediate usage.
    private List<PageAction> mResolvedPageActionCache;
    private PageActionLayoutDelegate mPageActionDelegate;

    private boolean mInitialized;

    public static AddonUICache getInstance() {
        return instance;
    }

    private AddonUICache() { }

    public void init() {
        if (mInitialized) {
            return;
        }

        EventDispatcher.getInstance().registerUiThreadListener(this,
            "Menu:Add",
            "Menu:Update",
            "Menu:Remove",
            "PageActions:Add",
            "PageActions:Remove",
            null);

        mInitialized = true;
    }

    @VisibleForTesting
    /* package, intended private */ void reset() {
        mAddonMenuItemsCache.clear();
        mAddonMenuNextID = ADDON_MENU_OFFSET;
        mMenu = null;
        mPendingPageActionQueue.clear();
        mResolvedPageActionCache = null;
        mPageActionDelegate = null;
    }

    @Override
    public void handleMessage(String event, GeckoBundle message, EventCallback callback) {
        switch (event) {
            case "Menu:Add":
                final MenuItemInfo info = new MenuItemInfo();
                info.label = message.getString("name");
                if (TextUtils.isEmpty(info.label)) {
                    Log.e(LOGTAG, "Invalid menu item name");
                    return;
                }
                info.id = mAddonMenuNextID++;
                info.uuid = message.getString("uuid");
                info.checked = message.getBoolean("checked", false);
                info.enabled = message.getBoolean("enabled", true);
                info.visible = message.getBoolean("visible", true);
                info.checkable = message.getBoolean("checkable", false);
                final String parentUUID = message.getString("parent");
                if (GECKO_TOOLS_MENU_UUID.equals(parentUUID)) {
                    info.parent = GECKO_TOOLS_MENU_ID;
                } else if (!TextUtils.isEmpty(parentUUID)) {
                    MenuItemInfo parent = mAddonMenuItemsCache.get(parentUUID);
                    if (parent != null) {
                        info.parent = parent.id;
                    }
                }
                addAddonMenuItem(info);
                break;

            case "Menu:Remove":
                removeAddonMenuItem(message.getString("uuid"));
                break;

            case "Menu:Update":
                updateAddonMenuItem(message.getString("uuid"),
                                    message.getBundle("options"));
                break;

            case "PageActions:Add":
                if (mPageActionDelegate != null) {
                    mPageActionDelegate.addPageAction(message);
                } else {
                    mPendingPageActionQueue.put(message.getString("id"), message);
                }
                break;

            case "PageActions:Remove":
                if (mPageActionDelegate != null) {
                    mPageActionDelegate.removePageAction(message);
                } else {
                    final String id = message.getString("id");
                    mPendingPageActionQueue.remove(id);
                    if (mResolvedPageActionCache != null) {
                        PageAction.removeFromList(mResolvedPageActionCache, id);
                    }
                }
                break;
        }
    }

    /**
     * If a list of {@link PageAction PageActions} has previously been provided in
     * {@link AddonUICache#removePageActionLayoutDelegate}, it will be transferred back to the
     * {@link PageActionLayoutDelegate}.
     * In addition, any <code>GeckoBundles</code> containing <code>PageAction</code> messages that
     * arrived while no delegate was available will now be transmitted.
     * <p>
     * Following this, any <code>PageAction</code> messages that arrive will be forwarded
     * immediately to the provided delegate.
     */
    public void setPageActionLayoutDelegate(final @NonNull PageActionLayoutDelegate newDelegate) {
        newDelegate.setCachedPageActions(mResolvedPageActionCache);
        mResolvedPageActionCache = null;

        for (GeckoBundle pageActionMessage : mPendingPageActionQueue.values()) {
            newDelegate.addPageAction(pageActionMessage);
        }
        mPendingPageActionQueue.clear();

        mPageActionDelegate = newDelegate;
    }

    /**
     * Clears the current PageActionDelegate and optionally takes a list of PageActions
     * that will be stored, e.g. if the class that provided the delegate is going away.
     *
     * In addition, all PageAction EventDispatcher messages that arrive while no delegate is
     * available will be stored for later retrieval.
     */
    public void removePageActionLayoutDelegate(final @Nullable List<PageAction> pageActionsToCache) {
        mPageActionDelegate = null;
        mResolvedPageActionCache = pageActionsToCache;
    }

    /**
     * Starts handling add-on menu items for the given {@link Menu} and also adds any
     * menu items that have already been cached.
     */
    public void onCreateOptionsMenu(Menu menu) {
        mMenu = menu;

        // Add add-on menu items, if any exist.
        if (mMenu != null) {
            for (MenuItemInfo item : mAddonMenuItemsCache.values()) {
                addAddonMenuItemToMenu(mMenu, item);
            }
        }
    }

    /**
     * Clears the reference to the Menu passed in {@link AddonUICache#onCreateOptionsMenu}.
     * <p>
     * Note: Any {@link MenuItem MenuItem(s)} previously added by this class are <i>not</i> removed.
     */
    public void onDestroyOptionsMenu() {
        mMenu = null;
    }

    /**
     * Adds an addon menu item/webextension browser action to the menu.
     */
    private void addAddonMenuItem(final MenuItemInfo info) {
        mAddonMenuItemsCache.put(info.uuid, info);

        if (mMenu == null) {
            return;
        }

        addAddonMenuItemToMenu(mMenu, info);
    }

    /**
     * Removes an addon menu item/webextension browser action from the menu by its UUID.
     */
    private void removeAddonMenuItem(String uuid) {
        // Remove add-on menu item from cache, if available.
        final MenuItemInfo item = mAddonMenuItemsCache.remove(uuid);

        if (mMenu == null || item == null) {
            return;
        }

        final MenuItem menuItem = mMenu.findItem(item.id);
        if (menuItem != null) {
            mMenu.removeItem(item.id);
        }
    }

    /**
     * Updates the addon menu/webextension browser action with the specified UUID.
     */
    private void updateAddonMenuItem(String uuid, final GeckoBundle options) {
        // Set attribute for the menu item in cache, if available
        final MenuItemInfo item = mAddonMenuItemsCache.get(uuid);
        if (item != null) {
            item.label = options.getString("name", item.label);
            item.checkable = options.getBoolean("checkable", item.checkable);
            item.checked = options.getBoolean("checked", item.checked);
            item.enabled = options.getBoolean("enabled", item.enabled);
            item.visible = options.getBoolean("visible", item.visible);
        }

        if (mMenu == null || item == null) {
            return;
        }

        final MenuItem menuItem = mMenu.findItem(item.id);
        if (menuItem != null) {
            menuItem.setTitle(options.getString("name", menuItem.getTitle().toString()));
            menuItem.setCheckable(options.getBoolean("checkable", menuItem.isCheckable()));
            menuItem.setChecked(options.getBoolean("checked", menuItem.isChecked()));
            menuItem.setEnabled(options.getBoolean("enabled", menuItem.isEnabled()));
            menuItem.setVisible(options.getBoolean("visible", menuItem.isVisible()));
        }
    }

    /**
     * Add the provided item to the provided menu, which should be
     * the root (mMenu).
     */
    private static void addAddonMenuItemToMenu(final Menu menu, final MenuItemInfo info) {
        final Menu destination;
        if (info.parent == 0) {
            destination = menu;
        } else if (info.parent == GECKO_TOOLS_MENU_ID) {
            // The tools menu only exists in our -v11 resources.
            final MenuItem tools = menu.findItem(R.id.tools);
            destination = tools != null ? tools.getSubMenu() : menu;
        } else {
            final MenuItem parent = menu.findItem(info.parent);
            if (parent == null) {
                return;
            }

            Menu parentMenu = findParentMenu(menu, parent);

            if (!parent.hasSubMenu()) {
                parentMenu.removeItem(parent.getItemId());
                destination = parentMenu.addSubMenu(Menu.NONE, parent.getItemId(), Menu.NONE, parent.getTitle());
                if (parent.getIcon() != null) {
                    ((SubMenu) destination).getItem().setIcon(parent.getIcon());
                }
            } else {
                destination = parent.getSubMenu();
            }
        }

        final MenuItem item = destination.add(Menu.NONE, info.id, Menu.NONE, info.label);

        item.setOnMenuItemClickListener(new MenuItem.OnMenuItemClickListener() {
            @Override
            public boolean onMenuItemClick(MenuItem item) {
                final GeckoBundle data = new GeckoBundle(1);
                data.putString("item", info.uuid);
                EventDispatcher.getInstance().dispatch("Menu:Clicked", data);
                return true;
            }
        });

        item.setCheckable(info.checkable);
        item.setChecked(info.checked);
        item.setEnabled(info.enabled);
        item.setVisible(info.visible);
    }

    private static Menu findParentMenu(Menu menu, MenuItem item) {
        final int itemId = item.getItemId();

        final int count = (menu != null) ? menu.size() : 0;
        for (int i = 0; i < count; i++) {
            MenuItem menuItem = menu.getItem(i);
            if (menuItem.getItemId() == itemId) {
                return menu;
            }
            if (menuItem.hasSubMenu()) {
                Menu parent = findParentMenu(menuItem.getSubMenu(), item);
                if (parent != null) {
                    return parent;
                }
            }
        }

        return null;
    }

}
