/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
    waitForExplicitFinish();

    Services.logins.removeAllLogins();

    // Add some initial logins
    let urls = [
        "http://example.com/",
        "http://mozilla.org/",
        "http://spreadfirefox.com/",
        "https://developer.mozilla.org/",
        "http://hg.mozilla.org/"
    ];
    let nsLoginInfo = new Components.Constructor("@mozilla.org/login-manager/loginInfo;1",
                                                 Ci.nsILoginInfo, "init");
    let logins = [
        new nsLoginInfo(urls[0], urls[0], null, "", "o hai", "u1", "p1"),
        new nsLoginInfo(urls[1], urls[1], null, "ehsan", "coded", "u2", "p2"),
        new nsLoginInfo(urls[2], urls[2], null, "this", "awesome", "u3", "p3"),
        new nsLoginInfo(urls[3], urls[3], null, "array of", "logins", "u4", "p4"),
        new nsLoginInfo(urls[4], urls[4], null, "then", "i wrote the test", "u5", "p5")
    ];
    logins.forEach(login => Services.logins.addLogin(login));

    // Open the password manager dialog
    const PWMGR_DLG = "chrome://passwordmgr/content/passwordManager.xul";
    let pwmgrdlg = window.openDialog(PWMGR_DLG, "Toolkit:PasswordManager", "");
    SimpleTest.waitForFocus(doTest, pwmgrdlg);

    // Test if "Copy Username" and "Copy Password" works
    function doTest() {
        let doc = pwmgrdlg.document;
        let selection = doc.getElementById("signonsTree").view.selection;
        let menuitem = doc.getElementById("context-copyusername");

        function copyField() {
            info("Select all");
            selection.selectAll();
            assertMenuitemEnabled("copyusername", false);
            assertMenuitemEnabled("editusername", false);
            assertMenuitemEnabled("copypassword", false);
            assertMenuitemEnabled("editpassword", false);

            info("Select the first row (with an empty username)");
            selection.select(0);
            assertMenuitemEnabled("copyusername", false, "empty username");
            assertMenuitemEnabled("editusername", true);
            assertMenuitemEnabled("copypassword", true);
            assertMenuitemEnabled("editpassword", true);

            info("Clear the selection");
            selection.clearSelection();
            assertMenuitemEnabled("copyusername", false);
            assertMenuitemEnabled("editusername", false);
            assertMenuitemEnabled("copypassword", false);
            assertMenuitemEnabled("editpassword", false);

            info("Select the third row and making the password column visible");
            selection.select(2);
            assertMenuitemEnabled("copyusername", true);
            assertMenuitemEnabled("editusername", true);
            assertMenuitemEnabled("copypassword", true);
            assertMenuitemEnabled("editpassword", true, "password column visible");
            menuitem.doCommand();
        }

        function assertMenuitemEnabled(idSuffix, expected, reason = "") {
            doc.defaultView.UpdateContextMenu();
            let actual = !doc.getElementById("context-" + idSuffix).getAttribute("disabled");
            is(actual, expected, idSuffix + " should be " + (expected ? "enabled" : "disabled") +
               (reason ? ": " + reason : ""));
        }

        function cleanUp() {
            Services.ww.registerNotification(function (aSubject, aTopic, aData) {
                Services.ww.unregisterNotification(arguments.callee);
                Services.logins.removeAllLogins();
                finish();
            });
            pwmgrdlg.close();
        }

        function testPassword() {
            info("Testing Copy Password");
            waitForClipboard("coded", function copyPassword() {
                menuitem = doc.getElementById("context-copypassword");
                menuitem.doCommand();
            }, cleanUp, cleanUp);
        }

        info("Testing Copy Username");
        waitForClipboard("ehsan", copyField, testPassword, testPassword);
    }
}
