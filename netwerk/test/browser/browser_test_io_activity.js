/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URL = "http://example.com/browser/dom/tests/browser/dummy.html";


add_task(async function test() {

    SpecialPowers.setIntPref('io.activity.intervalMilliseconds', 50);
    waitForExplicitFinish();

    // grab events..
    let gotSocket = false;
    let gotFile = false;
    let gotSqlite = false;
    let gotEmptyData = false;

    let networkActivity = function(subject, topic, value) {
        subject.QueryInterface(Ci.nsIMutableArray);
        let enumerator = subject.enumerate();
        while (enumerator.hasMoreElements()) {
            let data = enumerator.getNext();
            data = data.QueryInterface(Ci.nsIIOActivityData);
            gotEmptyData = data.rx == 0 && data.tx == 0 && !gotEmptyData
            gotSocket = data.location.startsWith("socket://127.0.0.1:") || gotSocket;
            gotFile = data.location.endsWith(".js") || gotFile;
            gotSqlite = data.location.endsWith("places.sqlite") || gotSqlite;
        }
    };

    Services.obs.addObserver(networkActivity, "io-activity");

    // why do I have to do this ??
    Services.obs.notifyObservers(null, "profile-initial-state", null);

    await BrowserTestUtils.withNewTab({ gBrowser, url: "http://example.com" },
      async function(browser) {
        // wait until we get the events back
        await BrowserTestUtils.waitForCondition(() => {
          return gotSocket && gotFile && gotSqlite && !gotEmptyData;
        }, "wait for events to come in", 250, 5);

        ok(gotSocket, "A socket was used");
        ok(gotFile, "A file was used");
        ok(gotSqlite, "A sqlite DB was used");
        ok(!gotEmptyData, "Every I/O event had data");
    });

    SpecialPowers.clearUserPref('io.activity.intervalMilliseconds');
});
