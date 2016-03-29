/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests.components;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertEquals;
import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertFalse;
import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertNotNull;
import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertTrue;

import java.util.List;
import java.util.concurrent.Callable;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.R;
import org.mozilla.gecko.menu.MenuItemActionBar;
import org.mozilla.gecko.menu.MenuItemDefault;
import org.mozilla.gecko.tests.UITestContext;
import org.mozilla.gecko.tests.helpers.DeviceHelper;
import org.mozilla.gecko.tests.helpers.RobotiumHelper;
import org.mozilla.gecko.tests.helpers.WaitHelper;

import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;
import android.widget.RelativeLayout;

import com.robotium.solo.Condition;
import com.robotium.solo.RobotiumUtils;
import com.robotium.solo.Solo;

/**
 * A class representing any interactions that take place on the app menu.
 */
public class AppMenuComponent extends BaseComponent {
    private static final int MAX_WAITTIME_FOR_MENU_UPDATE_IN_MS = 7500;

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

    public void assertMenuItemIsDisabledAndVisible(PageMenuItem pageMenuItem) {
        openAppMenu();

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
    private View findAppMenuItemView(final String text) {
        return WaitHelper.waitFor(String.format("menu item view '%s'", text), new Callable<View>() {
            @Override
            public View call() throws Exception {
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
                        if (relativeLayout instanceof RelativeLayout) {
                            View listMenuItemView = (View)relativeLayout.getParent();
                            return listMenuItemView;
                        }
                    }
                }
                return null;
            }
        }, MAX_WAITTIME_FOR_MENU_UPDATE_IN_MS);
    }

    /**
     * Helper function to let Robotium locate and click menu item from legacy Android menu (devices with Android 2.x).
     *
     * Robotium will also try to open the menu if there are no open dialog.
     *
     * @param menuItemTitle, The title of menu item to open.
     */
    private void pressLegacyMenuItem(final String menuItemTitle) {
        mSolo.clickOnMenuItem(menuItemTitle, true);
    }

    private void pressMenuItem(final String menuItemTitle) {
        // Wait for the menu item view to be enabled. This improves reliability on Android 2.3.
        WaitHelper.waitFor(String.format("menu item %s to be enabled", menuItemTitle), new Condition() {
            @Override
            public boolean isSatisfied() {
                View v = findAppMenuItemView(menuItemTitle);
                return (v != null) && v.isEnabled();
            }
        });

        final View menuItemView = findAppMenuItemView(menuItemTitle);
        fAssertTrue("Menu is open", isMenuOpen(menuItemView));

        fAssertTrue(String.format("The menu item %s is enabled", menuItemTitle), menuItemView.isEnabled());
        fAssertEquals(String.format("The menu item %s is visible", menuItemTitle), View.VISIBLE,
            menuItemView.getVisibility());

        mSolo.clickOnView(menuItemView);
    }

    private void pressSubMenuItem(final String parentMenuItemTitle, final String childMenuItemTitle) {
        openAppMenu();

        pressMenuItem(parentMenuItemTitle);

        // Child menu item is not pressed yet, Click on it.
        pressMenuItem(childMenuItemTitle);        
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
        if (DeviceHelper.isTablet()) {
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
    * Determines whether the app menu is open by searching for items in the menu.
    *
    * @return true if app menu is open.
    */
    private boolean isMenuOpen() {
        // We choose these options because New Tab is near the top of the menu and Page is near the middle/bottom.
        // Intermittently, the menu doesn't scroll to top so we can't just use the first item in the list.
        return isMenuOpen(MenuItem.NEW_TAB.getString(mSolo)) || isMenuOpen(MenuItem.PAGE.getString(mSolo));
    }

    /**
     * Determines whether the app menu is open by searching for the text in menuItemTitle.
     *
     * @param menuItemTitle, The contentDescription of menu item to search.
     *
     * @return true if app menu is open.
     */
    private boolean isMenuOpen(String menuItemTitle) {
        final View menuItemView = findAppMenuItemView(menuItemTitle);
        return isMenuOpen(menuItemView) ? true : RobotiumHelper.searchExactText(menuItemTitle, true);
    }

    /**
     * If a ListMenuItemView with menuItemTitle is visible then the app menu is open .
     *
     * @param menuItemView, must be a ListMenuItemView with menuItemTitle.
     *                      You must use findAppMenuItemView(menuItemTitle) to obtain it.
     *
     * @return true if app menu is open.
     */
    private boolean isMenuOpen(View menuItemView) {
        return (menuItemView != null) && (menuItemView.getVisibility() == View.VISIBLE);
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
