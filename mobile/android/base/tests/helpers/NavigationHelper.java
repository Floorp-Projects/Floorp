/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests.helpers;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertNotNull;

import org.mozilla.gecko.tests.UITestContext;
import org.mozilla.gecko.tests.UITestContext.ComponentType;
import org.mozilla.gecko.tests.components.AppMenuComponent;
import org.mozilla.gecko.tests.components.ToolbarComponent;

import com.jayway.android.robotium.solo.Solo;

/**
 * Provides helper functionality for navigating around the Firefox UI. These functions will often
 * combine actions taken on multiple components to perform larger interactions.
 */
final public class NavigationHelper {
    private static UITestContext sContext;
    private static Solo sSolo;

    private static AppMenuComponent sAppMenu;
    private static ToolbarComponent sToolbar;

    protected static void init(final UITestContext context) {
        sContext = context;
        sSolo = context.getSolo();

        sAppMenu = (AppMenuComponent) context.getComponent(ComponentType.APPMENU);
        sToolbar = (ToolbarComponent) context.getComponent(ComponentType.TOOLBAR);
    }

    public static void enterAndLoadUrl(String url) {
        fAssertNotNull("url is not null", url);

        url = adjustUrl(url);
        sToolbar.enterEditingMode()
                .enterUrl(url)
                .commitEditingMode();
    }

    /**
     * Returns a new URL with the docshell HTTP server host prefix.
     */
    private static String adjustUrl(final String url) {
        fAssertNotNull("url is not null", url);

        if (url.startsWith("about:") || url.startsWith("chrome:")) {
            return url;
        }

        return sContext.getAbsoluteHostnameUrl(url);
    }

    public static void goBack() {
        if (DeviceHelper.isTablet()) {
            sToolbar.pressBackButton(); // Waits for page load & asserts isNotEditing.
            return;
        }

        sToolbar.assertIsNotEditing();
        WaitHelper.waitForPageLoad(new Runnable() {
            @Override
            public void run() {
                // TODO: Lower soft keyboard first if applicable. Note that
                // Solo.hideSoftKeyboard() does not clear focus (which might be fine since
                // Gecko would be the element focused).
                sSolo.goBack();
            }
        });
    }

    public static void goForward() {
        if (DeviceHelper.isTablet()) {
            sToolbar.pressForwardButton(); // Waits for page load & asserts isNotEditing.
            return;
        }

        sToolbar.assertIsNotEditing();
        WaitHelper.waitForPageLoad(new Runnable() {
            @Override
            public void run() {
                sAppMenu.pressMenuItem(AppMenuComponent.MenuItem.FORWARD);
            }
        });
    }

    public static void reload() {
        if (DeviceHelper.isTablet()) {
            sToolbar.pressReloadButton(); // Waits for page load & asserts isNotEditing.
            return;
        }

        sToolbar.assertIsNotEditing();
        WaitHelper.waitForPageLoad(new Runnable() {
            @Override
            public void run() {
                sAppMenu.pressMenuItem(AppMenuComponent.MenuItem.RELOAD);
            }
        });
    }
}
