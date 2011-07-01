/* ***** BEGIN LICENSE BLOCK *****
 *   Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Frank Yan <fyan@mozilla.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

    // Test if "Copy Password" works
    function doTest() {
        let doc = pwmgrdlg.document;
        let selection = doc.getElementById("signonsTree").view.selection;
        let menuitem = doc.getElementById("context-copypassword");

        function copyPassword() {
            selection.selectAll();
            is(isMenuitemEnabled(), false, "Copy Password should be disabled");

            selection.select(0);
            is(isMenuitemEnabled(), true, "Copy Password should be enabled");

            selection.clearSelection();
            is(isMenuitemEnabled(), false, "Copy Password should be disabled");

            selection.select(2);
            is(isMenuitemEnabled(), true, "Copy Password should be enabled");
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

        waitForClipboard("coded", copyPassword, cleanUp, cleanUp);
    }
}
