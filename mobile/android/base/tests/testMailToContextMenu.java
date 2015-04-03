/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

public class testMailToContextMenu extends ContentContextMenuTest {

    // Test website strings
    private static String MAILTO_PAGE_URL;
    private static final String mailtoMenuItems [] = {"Copy Email Address", "Share Email Address"};

    public void testMailToContextMenu() {
        final String MAILTO_PAGE_TITLE = mStringHelper.ROBOCOP_BIG_MAILTO_TITLE;

        blockForGeckoReady();

        MAILTO_PAGE_URL=getAbsoluteUrl(mStringHelper.ROBOCOP_BIG_MAILTO_URL);
        loadUrlAndWait(MAILTO_PAGE_URL);
        waitForText(MAILTO_PAGE_TITLE);

        verifyContextMenuItems(mailtoMenuItems);
        verifyCopyOption(mailtoMenuItems[0], "foo.bar@example.com"); // Test the "Copy Email Address" option
        verifyShareOption(mailtoMenuItems[1], MAILTO_PAGE_TITLE); // Test the "Share Email Address" option
    }
}
