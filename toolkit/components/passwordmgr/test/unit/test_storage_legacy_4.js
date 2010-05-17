/*
 * Test suite for storage-Legacy.js -- mailnews specific tests.
 *
 * This test interfaces directly with the legacy login storage module,
 * bypassing the normal login manager usage.
 *
 */

const Cm = Components.manager;
const BASE_CONTRACTID = "@mozilla.org/network/protocol;1?name=";
const LDAPPH_CID = Components.ID("{08eebb58-8d1a-4ab5-9fca-e35372697828}");
const MAILBOXPH_CID = Components.ID("{edb1dea3-b226-405a-b93d-2a678a68a198}");
const NEWSBOXPH_CID = Components.ID("{939fe896-8961-49d0-b0e0-4ae779bdef36}");

const STORAGE_TYPE = "legacy";

function genericProtocolHandler(scheme, defaultPort) {
    this.scheme = scheme;
    this.defaultPort = defaultPort;
}

genericProtocolHandler.prototype = {
    scheme: "",
    defaultPort: 0,

    QueryInterface: function gph_QueryInterface(aIID) {
        if (!aIID.equals(Ci.nsISupports) &&
            !aIID.equals(Ci.nsIProtocolHandler)) {
            throw Cr.NS_ERROR_NO_INTERFACE;
        }
        return this;
    },

    get protocolFlags() {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
    },

    newURI: function gph_newURI(aSpec, anOriginalCharset, aBaseURI) {
        var uri = Components.classes["@mozilla.org/network/standard-url;1"].
                             createInstance(Ci.nsIStandardURL);

        uri.init(Ci.nsIStandardURL.URLTYPE_STANDARD, this.defaultPort, aSpec,
                  anOriginalCharset, aBaseURI);

        return uri;
    },

    newChannel: function gph_newChannel(aUri) {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
    },

    allowPort: function gph_allowPort(aPort, aScheme) {
        return false;
    }
}

function generateFactory(protocol, defaultPort)
{
    return {
        createInstance: function (outer, iid) {
            if (outer != null)
                throw Components.results.NS_ERROR_NO_AGGREGATION;

            return (new genericProtocolHandler(protocol, defaultPort)).
                    QueryInterface(iid);
        }
    };
}

function run_test() {
// News is set up as an external protocol in Firefox's prefs, for this test
// to work, we need it to be internal.
var prefBranch = Cc["@mozilla.org/preferences-service;1"].
                 getService(Ci.nsIPrefBranch);
prefBranch.setBoolPref("network.protocol-handler.external.news", false);

Cm.nsIComponentRegistrar.registerFactory(LDAPPH_CID, "LDAPProtocolFactory",
                                         BASE_CONTRACTID + "ldap",
                                         generateFactory("ldap", 389));
Cm.nsIComponentRegistrar.registerFactory(MAILBOXPH_CID,
                                         "MailboxProtocolFactory",
                                         BASE_CONTRACTID + "mailbox",
                                         generateFactory("mailbox", 0));
Cm.nsIComponentRegistrar.registerFactory(NEWSBOXPH_CID,
                                         "NewsProtocolFactory",
                                         BASE_CONTRACTID + "news",
                                         generateFactory("news", 119));

try {
/* ========== 0 ========== */
var testnum = 0;
var testdesc = "Initial connection to storage module";

var storage = Cc["@mozilla.org/login-manager/storage/legacy;1"].
              createInstance(Ci.nsILoginManagerStorage);
if (!storage)
    throw "Couldn't create storage instance.";

// Create a couple of dummy users to match what we expect to be translated
// from the input file.
var dummyuser1 = Cc["@mozilla.org/login-manager/loginInfo;1"].
                 createInstance(Ci.nsILoginInfo);
var dummyuser2 = Cc["@mozilla.org/login-manager/loginInfo;1"].
                 createInstance(Ci.nsILoginInfo);
var dummyuser3 = Cc["@mozilla.org/login-manager/loginInfo;1"].
                 createInstance(Ci.nsILoginInfo);
var dummyuser4 = Cc["@mozilla.org/login-manager/loginInfo;1"].
                 createInstance(Ci.nsILoginInfo);
var dummyuser5 = Cc["@mozilla.org/login-manager/loginInfo;1"].
                 createInstance(Ci.nsILoginInfo);
var dummyuser6 = Cc["@mozilla.org/login-manager/loginInfo;1"].
                 createInstance(Ci.nsILoginInfo);


dummyuser1.init("mailbox://localhost", null, "mailbox://localhost",
    "bugzilla", "testpass1", "", "");

dummyuser2.init("ldap://localhost1", null,
    "ldap://localhost1/dc=test",
    "", "testpass2", "", "");

dummyuser3.init("mailbox://localhost", null, "mailbox://localhost",
    "test+pop3", "pop3test", "", "");

dummyuser4.init("http://dummyhost.mozilla.org", "", null,
    "testuser1", "testpass1", "put_user_here", "put_pw_here");

dummyuser5.init("news://localhost", null, "news://localhost/#password",
    "", "newstest", "", "");

dummyuser6.init("news://localhost", null, "news://localhost/#username",
    "", "testnews", "", "");

/*
 * ---------------------- Bug 403790 ----------------------
 * Migrating mailnews style username/passwords
 */

/* ========== 1 ========== */
testnum++;

testdesc = "checking reading of mailnews-like old logins";
storage = LoginTest.initStorage(INDIR, "signons-403790.txt",
                               OUTDIR, "output-403790.txt");
// signons-403790.txt has one extra login that is invalid, and hence isn't
// shown here.
LoginTest.checkStorageData(storage, [], [dummyuser1, dummyuser2, dummyuser3,
                                         dummyuser5, dummyuser6]);

storage.addLogin(dummyuser4); // trigger a write
LoginTest.checkStorageData(storage, [],
                           [dummyuser1, dummyuser2, dummyuser3, dummyuser4,
                            dummyuser5, dummyuser6]);

testdesc = "[flush and reload for verification]";
storage = LoginTest.reloadStorage(OUTDIR, "output-403790.txt");
LoginTest.checkStorageData(storage, [],
                           [dummyuser1, dummyuser2, dummyuser3, dummyuser4,
                            dummyuser5, dummyuser6]);

/* ========== end ========== */
} catch (e) {
    throw ("FAILED in test #" + testnum + " -- " + testdesc + ": " + e);
}

};
