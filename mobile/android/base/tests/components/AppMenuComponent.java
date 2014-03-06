/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests.components;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.*;

import org.mozilla.gecko.menu.MenuItemActionBar;
import org.mozilla.gecko.menu.MenuItemDefault;
import org.mozilla.gecko.tests.helpers.*;
import org.mozilla.gecko.tests.UITestContext;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.R;

import com.jayway.android.robotium.solo.Condition;
import com.jayway.android.robotium.solo.RobotiumUtils;
import com.jayway.android.robotium.solo.Solo;

import android.view.View;
import java.util.List;

/**
 * A class representing any interactions that take place on the app menu.
 */
public class AppMenuComponent extends BaseComponent {
    public enum MenuItem {
        FORWARD(R.string.forward),
        NEW_TAB(R.string.new_tab);

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

    public AppMenuComponent(final UITestContext testContext) {
        super(testContext);
    }

    private void assertMenuIsNotOpen() {
        fAssertFalse("Menu is not open", isMenuOpen());
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

    public void pressMenuItem(MenuItem menuItem) {
        openAppMenu();

        final String text = menuItem.getString(mSolo);
        final View menuItemView = findAppMenuItemView(text);

        if (menuItemView != null) {
            fAssertTrue("The menu item is enabled", menuItemView.isEnabled());
            fAssertEquals("The menu item is visible", View.VISIBLE, menuItemView.getVisibility());

            mSolo.clickOnView(menuItemView);
        } else {
            // We could not find a view representing this menu item: Let's let Robotium try to
            // locate and click it in the legacy Android menu (devices with Android 2.x).
            //
            // Even though we already opened the menu to see if we can locate the menu item,
            // Robotium will also try to open the menu if it doesn't find an open dialog (Does
            // not happen in this case).
            mSolo.clickOnMenuItem(text, true);
        }
    }

    private void openAppMenu() {
        assertMenuIsNotOpen();

        if (HardwareUtils.hasMenuButton()) {
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

    private boolean isMenuOpen() {
        // The presence of the "New tab" menu item is our best guess about whether
        // the menu is open or not.
        return mSolo.searchText(MenuItem.NEW_TAB.getString(mSolo));
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
