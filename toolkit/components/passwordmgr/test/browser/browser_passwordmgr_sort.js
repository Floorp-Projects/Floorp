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
