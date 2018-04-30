/*
 * Tests notifications dispatched when modifying stored logins.
 */

var expectedNotification;
var expectedData;

var TestObserver = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver, Ci.nsISupportsWeakReference]),

  observe(subject, topic, data) {
    Assert.equal(topic, "passwordmgr-storage-changed");
    Assert.equal(data, expectedNotification);

    switch (data) {
        case "addLogin":
            Assert.ok(subject instanceof Ci.nsILoginInfo);
            Assert.ok(subject instanceof Ci.nsILoginMetaInfo);
            Assert.ok(expectedData.equals(subject)); // nsILoginInfo.equals()
            break;
        case "modifyLogin":
            Assert.ok(subject instanceof Ci.nsIArray);
            Assert.equal(subject.length, 2);
            var oldLogin = subject.queryElementAt(0, Ci.nsILoginInfo);
            var newLogin = subject.queryElementAt(1, Ci.nsILoginInfo);
            Assert.ok(expectedData[0].equals(oldLogin)); // nsILoginInfo.equals()
            Assert.ok(expectedData[1].equals(newLogin));
            break;
        case "removeLogin":
            Assert.ok(subject instanceof Ci.nsILoginInfo);
            Assert.ok(subject instanceof Ci.nsILoginMetaInfo);
            Assert.ok(expectedData.equals(subject)); // nsILoginInfo.equals()
            break;
        case "removeAllLogins":
            Assert.equal(subject, null);
            break;
        case "hostSavingEnabled":
        case "hostSavingDisabled":
            Assert.ok(subject instanceof Ci.nsISupportsString);
            Assert.equal(subject.data, expectedData);
            break;
        default:
            do_throw("Unhandled notification: " + data + " / " + topic);
    }

    expectedNotification = null; // ensure a duplicate is flagged as unexpected.
    expectedData = null;
  }
};

add_task(function test_notifications()
{

try {

var testnum = 0;
var testdesc = "Setup of nsLoginInfo test-users";

var testuser1 = new LoginInfo("http://testhost1", "", null,
    "dummydude", "itsasecret", "put_user_here", "put_pw_here");

var testuser2 = new LoginInfo("http://testhost2", "", null,
    "dummydude2", "itsasecret2", "put_user2_here", "put_pw2_here");

Services.obs.addObserver(TestObserver, "passwordmgr-storage-changed");


/* ========== 1 ========== */
testnum = 1;
testdesc = "Initial connection to storage module";

/* ========== 2 ========== */
testnum++;
testdesc = "addLogin";

expectedNotification = "addLogin";
expectedData = testuser1;
Services.logins.addLogin(testuser1);
LoginTestUtils.checkLogins([testuser1]);
Assert.equal(expectedNotification, null); // check that observer got a notification

/* ========== 3 ========== */
testnum++;
testdesc = "modifyLogin";

expectedNotification = "modifyLogin";
expectedData = [testuser1, testuser2];
Services.logins.modifyLogin(testuser1, testuser2);
Assert.equal(expectedNotification, null);
LoginTestUtils.checkLogins([testuser2]);

/* ========== 4 ========== */
testnum++;
testdesc = "removeLogin";

expectedNotification = "removeLogin";
expectedData = testuser2;
Services.logins.removeLogin(testuser2);
Assert.equal(expectedNotification, null);
LoginTestUtils.checkLogins([]);

/* ========== 5 ========== */
testnum++;
testdesc = "removeAllLogins";

expectedNotification = "removeAllLogins";
expectedData = null;
Services.logins.removeAllLogins();
Assert.equal(expectedNotification, null);
LoginTestUtils.checkLogins([]);

/* ========== 6 ========== */
testnum++;
testdesc = "removeAllLogins (again)";

expectedNotification = "removeAllLogins";
expectedData = null;
Services.logins.removeAllLogins();
Assert.equal(expectedNotification, null);
LoginTestUtils.checkLogins([]);

/* ========== 7 ========== */
testnum++;
testdesc = "setLoginSavingEnabled / false";

expectedNotification = "hostSavingDisabled";
expectedData = "http://site.com";
Services.logins.setLoginSavingEnabled("http://site.com", false);
Assert.equal(expectedNotification, null);
LoginTestUtils.assertDisabledHostsEqual(Services.logins.getAllDisabledHosts(),
                                        ["http://site.com"]);

/* ========== 8 ========== */
testnum++;
testdesc = "setLoginSavingEnabled / false (again)";

expectedNotification = "hostSavingDisabled";
expectedData = "http://site.com";
Services.logins.setLoginSavingEnabled("http://site.com", false);
Assert.equal(expectedNotification, null);
LoginTestUtils.assertDisabledHostsEqual(Services.logins.getAllDisabledHosts(),
                                        ["http://site.com"]);

/* ========== 9 ========== */
testnum++;
testdesc = "setLoginSavingEnabled / true";

expectedNotification = "hostSavingEnabled";
expectedData = "http://site.com";
Services.logins.setLoginSavingEnabled("http://site.com", true);
Assert.equal(expectedNotification, null);
LoginTestUtils.checkLogins([]);

/* ========== 10 ========== */
testnum++;
testdesc = "setLoginSavingEnabled / true (again)";

expectedNotification = "hostSavingEnabled";
expectedData = "http://site.com";
Services.logins.setLoginSavingEnabled("http://site.com", true);
Assert.equal(expectedNotification, null);
LoginTestUtils.checkLogins([]);

Services.obs.removeObserver(TestObserver, "passwordmgr-storage-changed");

LoginTestUtils.clearData();

} catch (e) {
    throw new Error("FAILED in test #" + testnum + " -- " + testdesc + ": " + e);
}

});
