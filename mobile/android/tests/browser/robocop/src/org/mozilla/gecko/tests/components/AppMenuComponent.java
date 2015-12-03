/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests.components;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertEquals;
import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertFalse;
import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertNotNull;
import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertTrue;

import java.util.List;

import org.mozilla.gecko.R;
import org.mozilla.gecko.menu.MenuItemActionBar;
import org.mozilla.gecko.menu.MenuItemDefault;
import org.mozilla.gecko.tests.UITestContext;
import org.mozilla.gecko.tests.helpers.DeviceHelper;
import org.mozilla.gecko.tests.helpers.WaitHelper;
import org.mozilla.gecko.util.HardwareUtils;

import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import com.jayway.android.robotium.solo.Condition;
import com.jayway.android.robotium.solo.RobotiumUtils;
import com.jayway.android.robotium.solo.Solo;

/**
 * A class representing any interactions that take place on the app menu.
 */
public class AppMenuComponent extends BaseComponent {
    private static final long MAX_WAITTIME_FOR_MENU_UPDATE_IN_MS = 1000L;

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
                fAssertNotNull("The page menu item is not null", pageMenuItemView);
                fAssertFalse("The page menu item is not enabled", pageMenuItemView.isEnabled());
                fAssertEquals("The page menu item is visible", View.VISIBLE, pageMenuItemView.getVisibility());
            } else {
                fAssertFalse("The parent 'page' menu item is not enabled", parentMenuItemView.isEnabled());
                fAssertEquals("The parent 'page' menu item is visible", View.VISIBLE, parentMenuItemView.getVisibility());
            }
        } else {
            // Legacy devices (Android 2.3 and earlier) don't have the parent menu, "Page", so check directly for the menu
            // item represented by pageMenuItem.
            //
            // We need to make sure the appropriate menu view is constructed
            // so open the "More" menu to additionally construct those views.
            openLegacyMoreMenu();

            final View pageMenuItemView = findAppMenuItemView(pageMenuItem.getString(mSolo));
            fAssertFalse("The page menu item is not enabled", pageMenuItemView.isEnabled());
            fAssertEquals("The page menu item is visible", View.VISIBLE, pageMenuItemView.getVisibility());

            // Close the "More" menu.
            mSolo.goBack();
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
     * When using legacy menus, make sure the menu has been opened to the appropriate level
     * (i.e. base menu or "More" menu) to ensure the appropriate menu views are in memory.
     * TODO: ^ Maybe we just need to have opened the "More" menu and the current one doesn't matter.
     *
     * This method is dependent on not having two views with equivalent contentDescription / text.
     */
    private View findAppMenuItemView(String text) {
        mSolo.waitForText(text, 1, MAX_WAITTIME_FOR_MENU_UPDATE_IN_MS);

        final List<View> views = mSolo.getViews();

        final List<MenuItemActionBar> menuItemActionBarList = RobotiumUtils.filterViews(MenuItemActionBar.class, views);
        for (MenuItemActionBar menuItem : menuItemActionBarList) {
            if (TextUtils.equals(menuItem.getContentDescription(), text)) {
                return menuItem;
            }
        }

        final List<MenuItemDefault> menuItemDefaultList = RobotiumUtils.filterViews(MenuItemDefault.class, views);
        for (MenuItemDefault menuItem : menuItemDefaultList) {
            if (TextUtils.equals(menuItem.getText(), text)) {
                return menuItem;
            }
        }

        // On Android 2.3, menu items may be instances of
        // com.android.internal.view.menu.ListMenuItemView, each with a child
        // android.widget.RelativeLayout which in turn has a child
        // TextView with the appropriate text.
        final List<TextView> textViewList = RobotiumUtils.filterViews(TextView.class, views);
        for (TextView textView : textViewList) {
            if (TextUtils.equals(textView.getText(), text)) {
                View relativeLayout = (View) textView.getParent();
                View listMenuItemView = (View)relativeLayout.getParent();
                return listMenuItemView;
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

    /**
     * Opens the "More" options menu on legacy Android devices. Assumes the base menu
     * (i.e. {@link #openAppMenu()}) has already been called and thus the menu is open.
     */
    private void openLegacyMoreMenu() {
        fAssertTrue("The base menu is already open", isMenuOpen());

        // Since there may be more views with "More" on the screen,
        // this is not robust. However, there may not be a better way.
        mSolo.clickOnText("^More$");

        WaitHelper.waitFor("legacy \"More\" menu to open", new Condition() {
            @Override
            public boolean isSatisfied() {
                return isLegacyMoreMenuOpen();
            }
        });
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

    private boolean isLegacyMoreMenuOpen() {
        // Check if the first menu option is visible.
        return mSolo.searchText(mSolo.getString(R.string.share), true);
    }

    /**
     * Determines whether the app menu is open by searching for the text in menuItemTitle.
     *
     * @param menuItemTitle, The contentDescription of menu item to search.
     *
     * @return true if app menu is open.
     */
    private boolean isMenuOpen(String menuItemTitle) {
        return mSolo.searchText(menuItemTitle, true);
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
