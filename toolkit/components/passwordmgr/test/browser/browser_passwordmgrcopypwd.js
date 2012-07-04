/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
    waitForExplicitFinish();

    let pwmgr = Cc["@mozilla.org/login-manager;1"].
                getService(Ci.nsILoginManager);
    pwmgr.removeAllLogins();

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
        new nsLoginInfo(urls[0], urls[0], null, "o", "hai", "u1", "p1"),
        new nsLoginInfo(urls[1], urls[1], null, "ehsan", "coded", "u2", "p2"),
        new nsLoginInfo(urls[2], urls[2], null, "this", "awesome", "u3", "p3"),
        new nsLoginInfo(urls[3], urls[3], null, "array of", "logins", "u4", "p4"),
        new nsLoginInfo(urls[4], urls[4], null, "then", "i wrote the test", "u5", "p5")
    ];
    logins.forEach(function (login) pwmgr.addLogin(login));

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
            selection.selectAll();
            is(isMenuitemEnabled(), false, "Copy should be disabled");

            selection.select(0);
            is(isMenuitemEnabled(), true, "Copy should be enabled");

            selection.clearSelection();
            is(isMenuitemEnabled(), false, "Copy should be disabled");

            selection.select(2);
            is(isMenuitemEnabled(), true, "Copy should be enabled");
            menuitem.doCommand();
        }

        function isMenuitemEnabled() {
            doc.defaultView.UpdateCopyPassword();
            return !menuitem.getAttribute("disabled");
        }

        function cleanUp() {
            Services.ww.registerNotification(function (aSubject, aTopic, aData) {
                Services.ww.unregisterNotification(arguments.callee);
                pwmgr.removeAllLogins();
                finish();
            });
            pwmgrdlg.close();
        }
        
        function testPassword() {
            menuitem = doc.getElementById("context-copypassword");
            info("Testing Copy Password");
            waitForClipboard("coded", copyField, cleanUp, cleanUp);
        }
        
        info("Testing Copy Username");
        waitForClipboard("ehsan", copyField, testPassword, testPassword);
    }
}
