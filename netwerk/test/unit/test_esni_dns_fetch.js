Cu.import("resource://gre/modules/NetUtil.jsm");

var prefs;
var h2Port;
var listen;

var dns = Cc["@mozilla.org/network/dns-service;1"].getService(Ci.nsIDNSService);
var threadManager = Cc["@mozilla.org/thread-manager;1"].getService(Ci.nsIThreadManager);
var mainThread = threadManager.currentThread;

const defaultOriginAttributes = {};

function run_test() {
  var env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  h2Port = env.get("MOZHTTP2_PORT");
  Assert.notEqual(h2Port, null);
  Assert.notEqual(h2Port, "");

  // Set to allow the cert presented by our H2 server
  do_get_profile();
  prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

  prefs.setBoolPref("network.security.esni.enabled", false);
  prefs.setBoolPref("network.http.spdy.enabled", true);
  prefs.setBoolPref("network.http.spdy.enabled.http2", true);
  // the TRR server is on 127.0.0.1
  prefs.setCharPref("network.trr.bootstrapAddress", "127.0.0.1");

  // make all native resolve calls "secretly" resolve localhost instead
  prefs.setBoolPref("network.dns.native-is-localhost", true);

  // 0 - off, 1 - race, 2 TRR first, 3 TRR only, 4 shadow
  prefs.setIntPref("network.trr.mode", 2); // TRR first
  prefs.setBoolPref("network.trr.wait-for-portal", false);
  // don't confirm that TRR is working, just go!
  prefs.setCharPref("network.trr.confirmationNS", "skip");

  // The moz-http2 cert is for foo.example.com and is signed by CA.cert.der
  // so add that cert to the trust list as a signing cert.  // the foo.example.com domain name.
  let certdb = Cc["@mozilla.org/security/x509certdb;1"]
      .getService(Ci.nsIX509CertDB);
  addCertFromFile(certdb, "CA.cert.der", "CTu,u,u");
  do_test_pending();
  run_dns_tests();
}

registerCleanupFunction(() => {
  prefs.clearUserPref("network.security.esni.enabled");
  prefs.clearUserPref("network.http.spdy.enabled");
  prefs.clearUserPref("network.http.spdy.enabled.http2");
  prefs.clearUserPref("network.dns.localDomains");
  prefs.clearUserPref("network.dns.native-is-localhost");
  prefs.clearUserPref("network.trr.mode");
  prefs.clearUserPref("network.trr.uri");
  prefs.clearUserPref("network.trr.credentials");
  prefs.clearUserPref("network.trr.wait-for-portal");
  prefs.clearUserPref("network.trr.allow-rfc1918");
  prefs.clearUserPref("network.trr.useGET");
  prefs.clearUserPref("network.trr.confirmationNS");
  prefs.clearUserPref("network.trr.bootstrapAddress");
  prefs.clearUserPref("network.trr.blacklist-duration");
  prefs.clearUserPref("network.trr.request-timeout");

});

function readFile(file) {
  let fstream = Cc["@mozilla.org/network/file-input-stream;1"]
                  .createInstance(Ci.nsIFileInputStream);
  fstream.init(file, -1, 0, 0);
  let data = NetUtil.readInputStreamToString(fstream, fstream.available());
  fstream.close();
  return data;
}

function addCertFromFile(certdb, filename, trustString) {
  let certFile = do_get_file(filename, false);
  let der = readFile(certFile);
  certdb.addCert(der, trustString);
}

var test_answer="bXkgdm9pY2UgaXMgbXkgcGFzc3dvcmQ=";
var test_answer_addr="127.0.0.1";

// check that we do lookup by type fine
var listenerEsni = {
  onLookupByTypeComplete: function(inRequest, inRecord, inStatus) {
    if (inRequest == listen) {
      Assert.ok(!inStatus);
      var answer = inRecord.getRecordsAsOneString();
      Assert.equal(answer, test_answer);
      do_test_finished();
      run_dns_tests();
    }
  },
  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsIDNSListener) ||
      aIID.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

// check that we do lookup for A record is fine
var listenerAddr = {
  onLookupComplete: function(inRequest, inRecord, inStatus) {
    if (inRequest == listen) {
      Assert.ok(!inStatus);
      var answer = inRecord.getNextAddrAsString();
      Assert.equal(answer, test_answer_addr);
      do_test_finished();
      run_dns_tests();
    }
  },
  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsIDNSListener) ||
      aIID.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

function testEsniRequest()
{
  // use the h2 server as DOH provider
  prefs.setCharPref("network.trr.uri", "https://foo.example.com:" + h2Port + "/esni-dns");
  listen = dns.asyncResolveByType("_esni.example.com", dns.RESOLVE_TYPE_TXT, 0, listenerEsni, mainThread, defaultOriginAttributes);
}

// verify esni record pushed on a A record request
function testEsniPushPart1()
{
  prefs.setCharPref("network.trr.uri", "https://foo.example.com:" + h2Port + "/esni-dns-push");
  listen = dns.asyncResolve("_esni_push.example.com", 0, listenerAddr, mainThread, defaultOriginAttributes);
}

// verify the esni pushed record
function testEsniPushPart2()
{
  // At this point the second host name should've been pushed and we can resolve it using
  // cache only. Set back the URI to a path that fails.
  prefs.setCharPref("network.trr.uri", "https://foo.example.com:" + h2Port + "/404");
  listen = dns.asyncResolveByType("_esni_push.example.com", dns.RESOLVE_TYPE_TXT, 0, listenerEsni, mainThread, defaultOriginAttributes);
}

function testsDone()
{
  do_test_finished();
  do_test_finished();
}

var tests = [testEsniRequest,
             testEsniPushPart1,
             testEsniPushPart2,
             testsDone
            ];
var current_test = 0;

function run_dns_tests()
{
  if (current_test < tests.length) {
    dump("starting test " + current_test + "\n");
    do_test_pending();
    tests[current_test++]();
  }
}


