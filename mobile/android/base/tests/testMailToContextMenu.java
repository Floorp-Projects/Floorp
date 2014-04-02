package org.mozilla.gecko.tests;


public class testMailToContextMenu extends ContentContextMenuTest {

    // Test website strings
    private static String MAILTO_PAGE_URL;
    private static final String MAILTO_PAGE_TITLE = "Big Mailto";
    private static final String mailtoMenuItems [] = {"Copy Email Address", "Share Email Address"};

    public void testMailToContextMenu() {
        blockForGeckoReady();

        MAILTO_PAGE_URL=getAbsoluteUrl("/robocop/robocop_big_mailto.html");
        inputAndLoadUrl(MAILTO_PAGE_URL);
        waitForText(MAILTO_PAGE_TITLE);

        verifyContextMenuItems(mailtoMenuItems);
        verifyCopyOption(mailtoMenuItems[0], "foo.bar@example.com"); // Test the "Copy Email Address" option
        verifyShareOption(mailtoMenuItems[1], MAILTO_PAGE_TITLE); // Test the "Share Email Address" option
    }
}
