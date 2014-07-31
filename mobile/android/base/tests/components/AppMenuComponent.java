/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests.components;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertEquals;
import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertFalse;
import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertTrue;

import java.util.List;

import org.mozilla.gecko.R;
import org.mozilla.gecko.menu.MenuItemActionBar;
import org.mozilla.gecko.menu.MenuItemDefault;
import org.mozilla.gecko.tests.UITestContext;
import org.mozilla.gecko.tests.helpers.DeviceHelper;
import org.mozilla.gecko.tests.helpers.WaitHelper;
import org.mozilla.gecko.util.HardwareUtils;

import android.view.View;

import com.jayway.android.robotium.solo.Condition;
import com.jayway.android.robotium.solo.RobotiumUtils;
import com.jayway.android.robotium.solo.Solo;

/**
 * A class representing any interactions that take place on the app menu.
 */
public class AppMenuComponent extends BaseComponent {
    private Boolean hasLegacyMenu = null;

    public enum MenuItem {
        FORWARD(R.string.forward),
        NEW_TAB(R.string.new_tab),
        PAGE(R.string.page),
        RELOAD(R.string.reload);

        private final int resourceID;
        private String stringResource;

        MenuItem(final int resourceID) {
            this.resourceID = resourceID;
        }

        public String getString(final Solo solo) {
            if (stringResource == null) {
                stringResource = solo.getString(resourceID);
            }

            return stringResource;
        }
    };

    public enum PageMenuItem {
        SAVE_AS_PDF(R.string.save_as_pdf);

        private static final MenuItem PARENT_MENU = MenuItem.PAGE;

        private final int resourceID;
        private String stringResource;

        PageMenuItem(final int resourceID) {
            this.resourceID = resourceID;
        }

        public String getString(final Solo solo) {
            if (stringResource == null) {
                stringResource = solo.getString(resourceID);
            }

            return stringResource;
        }
    };

    public AppMenuComponent(final UITestContext testContext) {
        super(testContext);
    }

    private void assertMenuIsNotOpen() {
        fAssertFalse("Menu is not open", isMenuOpen());
    }

    /**
     * Legacy Android devices doesn't have hierarchical menus. Sub-menus, such as "Page", are missing in these devices.
     * Try to determine if the menu item "Page" is present.
     *
     * TODO : This fragile way to determine legacy menus should be replaced with a check for 6-panel menu item.
     *
     * @return true if there is a legacy menu.
     */
    private boolean hasLegacyMenu() {
        if (hasLegacyMenu == null) {
            hasLegacyMenu = findAppMenuItemView(MenuItem.PAGE.getString(mSolo)) == null;
        }

        return hasLegacyMenu;
    }

    public void assertMenuItemIsDisabledAndVisible(PageMenuItem pageMenuItem) {
        openAppMenu();

        if (!hasLegacyMenu()) {
            // Non-legacy devices have hierarchical menu, check for parent menu item "page".
            final View parentMenuItemView = findAppMenuItemView(MenuItem.PAGE.getString(mSolo));
            if (parentMenuItemView.isEnabled()) {
                fAssertTrue("The parent 'page' menu item is enabled", parentMenuItemView.isEnabled());
                fAssertEquals("The parent 'page' menu item is visible", View.VISIBLE,
                        parentMenuItemView.getVisibility());

                // Parent menu "page" is enabled, open page menu and check for menu item represented by pageMenuItem.
                pressMenuItem(MenuItem.PAGE.getString(mSolo));

                final View pageMenuItemView = findAppMenuItemView(pageMenuItem.getString(mSolo));
                fAssertFalse("The page menu item is not enabled", pageMenuItemView.isEnabled());
                fAssertEquals("The page menu item is visible", View.VISIBLE, pageMenuItemView.getVisibility());
            } else {
                fAssertFalse("The parent 'page' menu item is not enabled", parentMenuItemView.isEnabled());
                fAssertEquals("The parent 'page' menu item is visible", View.VISIBLE, parentMenuItemView.getVisibility());
            }
        } else {
            // Legacy devices don't have parent menu item "page", check for menu item represented by pageMenuItem.
            final View pageMenuItemView = findAppMenuItemView(pageMenuItem.getString(mSolo));
            fAssertFalse("The page menu item is not enabled", pageMenuItemView.isEnabled());
            fAssertEquals("The page menu item is visible", View.VISIBLE, pageMenuItemView.getVisibility());
        }

        // Close the App Menu.
        mSolo.goBack();
    }

    private View getOverflowMenuButtonView() {
        return mSolo.getView(R.id.menu);
    }

    /**
     * Try to find a MenuItemActionBar/MenuItemDefault with the given text set as contentDescription / text.
     *
     * Will return null when the Android legacy menu is in use.
     *
     * This method is dependent on not having two views with equivalent contentDescription / text.
     */
    private View findAppMenuItemView(String text) {
        final List<View> views = mSolo.getViews();

        final List<MenuItemActionBar> menuItemActionBarList = RobotiumUtils.filterViews(MenuItemActionBar.class, views);
        for (MenuItemActionBar menuItem : menuItemActionBarList) {
            if (menuItem.getContentDescription().equals(text)) {
                return menuItem;
            }
        }

        final List<MenuItemDefault> menuItemDefaultList = RobotiumUtils.filterViews(MenuItemDefault.class, views);
        for (MenuItemDefault menuItem : menuItemDefaultList) {
            if (menuItem.getText().equals(text)) {
                return menuItem;
            }
        }

        return null;
    }

    /**
     * Helper function to let Robotium locate and click menu item from legacy Android menu (devices with Android 2.x).
     *
     * Robotium will also try to open the menu if there are no open dialog.
     *
     * @param menuItemText, The title of menu item to open.
     */
    private void pressLegacyMenuItem(final String menuItemTitle) {
        mSolo.clickOnMenuItem(menuItemTitle, true);
    }

    private void pressMenuItem(final String menuItemTitle) {
        fAssertTrue("Menu is open", isMenuOpen(menuItemTitle));

        if (!hasLegacyMenu()) {
            final View menuItemView = findAppMenuItemView(menuItemTitle);

            fAssertTrue(String.format("The menu item %s is enabled", menuItemTitle), menuItemView.isEnabled());
            fAssertEquals(String.format("The menu item %s is visible", menuItemTitle), View.VISIBLE,
                    menuItemView.getVisibility());

            mSolo.clickOnView(menuItemView);
        } else {
            pressLegacyMenuItem(menuItemTitle);
        }
    }

    private void pressSubMenuItem(final String parentMenuItemTitle, final String childMenuItemTitle) {
        openAppMenu();

        if (!hasLegacyMenu()) {
            pressMenuItem(parentMenuItemTitle);

            // Child menu item is not pressed yet, Click on it.
            pressMenuItem(childMenuItemTitle);
        } else {
            pressLegacyMenuItem(childMenuItemTitle);
        }
    }

    public void pressMenuItem(MenuItem menuItem) {
        openAppMenu();
        pressMenuItem(menuItem.getString(mSolo));
    }

    public void pressMenuItem(final PageMenuItem pageMenuItem) {
        pressSubMenuItem(PageMenuItem.PARENT_MENU.getString(mSolo), pageMenuItem.getString(mSolo));
    }

    private void openAppMenu() {
        assertMenuIsNotOpen();

        // This is a hack needed for tablets where the OverflowMenuButton is always in the GONE state,
        // so we press the menu key instead.
        if (HardwareUtils.hasMenuButton() || DeviceHelper.isTablet()) {
            mSolo.sendKey(Solo.MENU);
        } else {
            pressOverflowMenuButton();
        }

        waitForMenuOpen();
    }

    private void pressOverflowMenuButton() {
        final View overflowMenuButton = getOverflowMenuButtonView();

        fAssertTrue("The overflow menu button is enabled", overflowMenuButton.isEnabled());
        fAssertEquals("The overflow menu button is visible", View.VISIBLE, overflowMenuButton.getVisibility());

        mSolo.clickOnView(overflowMenuButton, true);
    }

    /**
    * Determines whether the app menu is open by searching for the text "New tab".
    *
    * @return true if app menu is open.
    */
    private boolean isMenuOpen() {
        return isMenuOpen(MenuItem.NEW_TAB.getString(mSolo));
    }

    /**
     * Determines whether the app menu is open by searching for the text in menuItemTitle.
     *
     * @param menuItemTitle, The contentDescription of menu item to search.
     *
     * @return true if app menu is open.
     */
    private boolean isMenuOpen(String menuItemTitle) {
        return mSolo.searchText(menuItemTitle);
    }

    private void waitForMenuOpen() {
        WaitHelper.waitFor("menu to open", new Condition() {
            @Override
            public boolean isSatisfied() {
                return isMenuOpen();
            }
        });
    }
}
