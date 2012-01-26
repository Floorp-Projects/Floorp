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
 * Ehsan Akhgari.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com> (Original Author)
 *   Jens Hatlak <jh@junetz.de>
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
        "http://example.org/",
        "http://mozilla.com/",
        "http://mozilla.org/",
        "http://spreadfirefox.com/",
        "http://planet.mozilla.org/",
        "https://developer.mozilla.org/",
        "http://hg.mozilla.org/",
        "http://mxr.mozilla.org/",
        "http://feeds.mozilla.org/",
    ];
    let users = [
        "user",
        "username",
        "ehsan",
        "ehsan",
        "john",
        "what?",
        "really?",
        "you sure?",
        "my user name",
        "my username",
    ];
    let pwds = [
        "password",
        "password",
        "mypass",
        "mypass",
        "smith",
        "very secret",
        "super secret",
        "absolutely",
        "mozilla",
        "mozilla.com",
    ];
    let nsLoginInfo = new Components.Constructor("@mozilla.org/login-manager/loginInfo;1",
                                                 Ci.nsILoginInfo, "init");
    for (let i = 0; i < 10; i++)
        pwmgr.addLogin(new nsLoginInfo(urls[i], urls[i], null, users[i], pwds[i],
                                       "u"+(i+1), "p"+(i+1)));

    // Open the password manager dialog
    const PWMGR_DLG = "chrome://passwordmgr/content/passwordManager.xul";
    let pwmgrdlg = window.openDialog(PWMGR_DLG, "Toolkit:PasswordManager", "");
    SimpleTest.waitForFocus(doTest, pwmgrdlg);

    // the meat of the test
    function doTest() {
        let doc = pwmgrdlg.document;
        let win = doc.defaultView;
        let sTree = doc.getElementById("signonsTree");
        let filter = doc.getElementById("filter");
        let siteCol = doc.getElementById("siteCol");
        let userCol = doc.getElementById("userCol");
        let passwordCol = doc.getElementById("passwordCol");

        let toggleCalls = 0;
        function toggleShowPasswords(func) {
            let toggleButton = doc.getElementById("togglePasswords");
            let showMode = (toggleCalls++ % 2) == 0;

            // only watch for a confirmation dialog every other time being called
            if (showMode) {
                Services.ww.registerNotification(function (aSubject, aTopic, aData) {
                    if (aTopic == "domwindowclosed")
                        Services.ww.unregisterNotification(arguments.callee);
                    else if (aTopic == "domwindowopened") {
                        let win = aSubject.QueryInterface(Ci.nsIDOMEventTarget);
                        SimpleTest.waitForFocus(function() {
                            EventUtils.sendKey("RETURN", win);
                        }, win);
                    }
                });
            }

            Services.obs.addObserver(function (aSubject, aTopic, aData) {
                if (aTopic == "passwordmgr-password-toggle-complete") {
                    Services.obs.removeObserver(arguments.callee, aTopic, false);
                    func();
                }
            }, "passwordmgr-password-toggle-complete", false);

            EventUtils.synthesizeMouse(toggleButton, 1, 1, {}, win);
        }

        function clickCol(col) {
            EventUtils.synthesizeMouse(col, 20, 1, {}, win);
            setTimeout(runNextTest, 0);
        }

        function setFilter(string) {
            filter.value = string;
            filter.doCommand();
            setTimeout(runNextTest, 0);
        }

        function checkSortMarkers(activeCol) {
            let isOk = true;
            let col = null;
            let hasAttr = false;
            let treecols = activeCol.parentNode;
            for (let i = 0; i < treecols.childNodes.length; i++) {
                col = treecols.childNodes[i];
                if (col.nodeName != "treecol")
                    continue;
                hasAttr = col.hasAttribute("sortDirection");
                isOk &= col == activeCol ? hasAttr : !hasAttr;
            }
            ok(isOk, "Only " + activeCol.id + " has a sort marker");
        }

        function checkSortDirection(col, ascending) {
            checkSortMarkers(col);
            let direction = ascending ? "ascending" : "descending";
            is(col.getAttribute("sortDirection"), direction,
               col.id + ": sort direction is " + direction);
        }

        function checkColumnEntries(aCol, expectedValues) {
            let actualValues = getColumnEntries(aCol);
            is(actualValues.length, expectedValues.length, "Checking length of expected column");
            for (let i = 0; i < expectedValues.length; i++)
                is(actualValues[i], expectedValues[i], "Checking column entry #"+i);
        }

        function getColumnEntries(aCol) {
            let entries = [];
            let column = sTree.columns[aCol];
            let numRows = sTree.view.rowCount;
            for (let i = 0; i < numRows; i++)
                entries.push(sTree.view.getCellText(i, column));
            return entries;
        }

        let testCounter = 0;
        let expectedValues;
        function runNextTest() {
            switch (testCounter++) {
            case 0:
                expectedValues = urls.slice().sort();
                checkColumnEntries(0, expectedValues);
                checkSortDirection(siteCol, true);
                // Toggle sort direction on Host column
                clickCol(siteCol);
                break;
            case 1:
                expectedValues.reverse();
                checkColumnEntries(0, expectedValues);
                checkSortDirection(siteCol, false);
                // Sort by Username
                clickCol(userCol);
                break;
            case 2:
                expectedValues = users.slice().sort();
                checkColumnEntries(1, expectedValues);
                checkSortDirection(userCol, true);
                // Sort by Password
                clickCol(passwordCol);
                break;
            case 3:
                expectedValues = pwds.slice().sort();
                checkColumnEntries(2, expectedValues);
                checkSortDirection(passwordCol, true);
                // Set filter
                setFilter("moz");
                break;
            case 4:
                expectedValues = [ "absolutely", "mozilla", "mozilla.com",
                                   "mypass", "mypass", "super secret",
                                   "very secret" ];
                checkColumnEntries(2, expectedValues);
                checkSortDirection(passwordCol, true);
                // Reset filter
                setFilter("");
                break;
            case 5:
                expectedValues = pwds.slice().sort();
                checkColumnEntries(2, expectedValues);
                checkSortDirection(passwordCol, true);
                // cleanup
                Services.ww.registerNotification(function (aSubject, aTopic, aData) {
                    // unregister ourself
                    Services.ww.unregisterNotification(arguments.callee);

                    pwmgr.removeAllLogins();
                    finish();
                });
                pwmgrdlg.close();
            }
        }

        // Toggle Show Passwords to display Password column, then start tests
        toggleShowPasswords(runNextTest);
    }
}
