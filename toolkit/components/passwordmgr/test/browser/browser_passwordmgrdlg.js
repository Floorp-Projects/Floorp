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
    let nsLoginInfo = new Components.Constructor("@mozilla.org/login-manager/loginInfo;1",
                                                 Ci.nsILoginInfo, "init");
    let logins = [
        new nsLoginInfo(urls[0], urls[0], null, "user", "password", "u1", "p1"),
        new nsLoginInfo(urls[1], urls[1], null, "username", "password", "u2", "p2"),
        new nsLoginInfo(urls[2], urls[2], null, "ehsan", "mypass", "u3", "p3"),
        new nsLoginInfo(urls[3], urls[3], null, "ehsan", "mypass", "u4", "p4"),
        new nsLoginInfo(urls[4], urls[4], null, "john", "smith", "u5", "p5"),
        new nsLoginInfo(urls[5], urls[5], null, "what?", "very secret", "u6", "p6"),
        new nsLoginInfo(urls[6], urls[6], null, "really?", "super secret", "u7", "p7"),
        new nsLoginInfo(urls[7], urls[7], null, "you sure?", "absolutely", "u8", "p8"),
        new nsLoginInfo(urls[8], urls[8], null, "my user name", "mozilla", "u9", "p9"),
        new nsLoginInfo(urls[9], urls[9], null, "my username", "mozilla.com", "u10", "p10"),
    ];
    logins.forEach(login => pwmgr.addLogin(login));

    // Open the password manager dialog
    const PWMGR_DLG = "chrome://passwordmgr/content/passwordManager.xul";
    let pwmgrdlg = window.openDialog(PWMGR_DLG, "Toolkit:PasswordManager", "");
    SimpleTest.waitForFocus(doTest, pwmgrdlg);

    // the meat of the test
    function doTest() {
        let doc = pwmgrdlg.document;
        let win = doc.defaultView;
        let filter = doc.getElementById("filter");
        let tree = doc.getElementById("signonsTree");
        let view = tree.view;

        is(filter.value, "", "Filter box should initially be empty");
        is(view.rowCount, 10, "There should be 10 passwords initially");

        // Prepare a set of tests
        //   filter: the text entered in the filter search box
        //   count: the number of logins which should match the respective filter
        let tests = [
            {filter: "pass", count: 4},
            {filter: "", count: 10}, // test clearing the filter
            {filter: "moz", count: 7},
            {filter: "mozi", count: 7},
            {filter: "mozil", count: 7},
            {filter: "mozill", count: 7},
            {filter: "mozilla", count: 7},
            {filter: "mozilla.com", count: 2},
            {filter: "user", count: 4},
            {filter: "user ", count: 1},
            {filter: " user", count: 2},
            {filter: "http", count: 10},
            {filter: "https", count: 1},
            {filter: "secret", count: 2},
            {filter: "secret!", count: 0},
        ];

        function runTests(endFunction) {
            let testCounter = 0;

            function setFilter(string) {
                filter.value = string;
                filter.doCommand();
            }

            function runOneTest(test) {
                is(view.rowCount, test.count, test.count + " logins should match '" + test.filter + "'");
                proceed();
            }

            function proceed() {
                // run the next test if necessary or proceed with the tests
                if (testCounter != tests.length)
                    runNextTest();
                else
                    endFunction();
            }

            function runNextTest() {
                let test = tests[testCounter++];
                setFilter(test.filter);
                setTimeout(runOneTest, 0, test);
            }

            runNextTest();
        }

        function step1() {
            runTests(lastStep);
        }

        function lastStep() {
            // cleanup
            Services.ww.registerNotification(function (aSubject, aTopic, aData) {
                // unregister ourself
                Services.ww.unregisterNotification(arguments.callee);

                pwmgr.removeAllLogins();
                finish();
            });
            pwmgrdlg.close();
        }

        step1();
    }
}
