Cu.import("resource://gre/modules/NetUtil.jsm");

var prefs;
var origin;
var h2Port;

var dns = Cc["@mozilla.org/network/dns-service;1"].getService(Ci.nsIDNSService);
var threadManager = Cc["@mozilla.org/thread-manager;1"].getService(Ci.nsIThreadManager);
var mainThread = threadManager.currentThread;

const defaultOriginAttributes = {};

function run_test() {
  dump ("start!\n");

  var env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  h2Port = env.get("MOZHTTP2_PORT");
  Assert.notEqual(h2Port, null);
  Assert.notEqual(h2Port, "");

  // Set to allow the cert presented by our H2 server
  do_get_profile();
  prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

  prefs.setBoolPref("network.http.spdy.enabled", true);
  prefs.setBoolPref("network.http.spdy.enabled.http2", true);
  // the TRR server is on 127.0.0.1
  prefs.setCharPref("network.trr.bootstrapAddress", "127.0.0.1");

  // use the h2 server as DOH provider
  prefs.setCharPref("network.trr.uri", "https://foo.example.com:" + h2Port + "/dns");
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

function resetTRRPrefs() {
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
}

registerCleanupFunction(() => {
  prefs.clearUserPref("network.http.spdy.enabled");
  prefs.clearUserPref("network.http.spdy.enabled.http2");
  prefs.clearUserPref("network.dns.localDomains");
  prefs.clearUserPref("network.dns.native-is-localhost");
  resetTRRPrefs();
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

function testsDone()
{
  do_test_finished();
  do_test_finished();
}

var test_loops;
var test_answer="127.0.0.1";

// check that we do lookup the name fine
var listenerFine = {
  onLookupComplete: function(inRequest, inRecord, inStatus) {
    if (inRequest == listen) {
      Assert.ok(!inStatus);
      var answer = inRecord.getNextAddrAsString();
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

// check that the name lookup fails
var listenerFails = {
  onLookupComplete: function(inRequest, inRecord, inStatus) {
    if (inRequest == listen) {
      Assert.ok(!Components.isSuccessCode(inStatus));
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

// check that we do lookup the name fine
var listenerUntilFine = {
  onLookupComplete: function(inRequest, inRecord, inStatus) {
    if ((inRequest == listen) && (inRecord != null)) {
      var answer = inRecord.getNextAddrAsString();
      if (answer == test_answer) {
        Assert.equal(answer, test_answer);
        dump("Got what we were waiting for!\n");
      }
    }
    else {
      // not the one we want, try again
      dump("Waiting for " + test_answer + " but got " + answer + "\n");
      --test_loops;
      Assert.ok(test_loops != 0);
      current_test--;
    }
    do_test_finished();
    run_dns_tests();
  },
  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsIDNSListener) ||
        aIID.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

var listen;

// verify basic A record
function test1()
{
  prefs.setIntPref("network.trr.mode", 2); // TRR-first
  prefs.setCharPref("network.trr.uri", "https://foo.example.com:" + h2Port + "/dns");
  test_answer="127.0.0.1";
  listen = dns.asyncResolve("bar.example.com", 0, listenerFine, mainThread, defaultOriginAttributes);
}

// verify basic A record - without bootstrapping
function test1b()
{
  prefs.setIntPref("network.trr.mode", 3); // TRR-only
  prefs.setCharPref("network.trr.uri", "https://foo.example.com:" + h2Port + "/dns");
  prefs.clearUserPref("network.trr.bootstrapAddress");
  prefs.setCharPref("network.dns.localDomains", "foo.example.com");
  test_answer = "127.0.0.1";
  listen = dns.asyncResolve("bar.example.com", 0, listenerFine, mainThread, defaultOriginAttributes);
}

// verify that the name was put in cache - it works with bad DNS URI
function test2()
{
  prefs.setIntPref("network.trr.mode", 3); // TRR-only
  //prefs.clearUserPref("network.trr.bootstrapAddress");
  //prefs.setCharPref("network.dns.localDomains", "foo.example.com");
  prefs.setCharPref("network.trr.uri", "https://foo.example.com:" + h2Port + "/404");
  test_answer = "127.0.0.1";
  listen = dns.asyncResolve("bar.example.com", 0, listenerFine, mainThread, defaultOriginAttributes);
}

// verify working credentials in DOH request
function test3()
{
  prefs.setIntPref("network.trr.mode", 3); // TRR-only
  prefs.setCharPref("network.trr.uri", "https://foo.example.com:" + h2Port + "/dns-auth");
  prefs.setCharPref("network.trr.credentials", "user:password");
  test_answer = "127.0.0.1";
  listen = dns.asyncResolve("bar.example.com", 0, listenerFine, mainThread, defaultOriginAttributes);
}

// verify failing credentials in DOH request
function test4()
{
  prefs.setIntPref("network.trr.mode", 3); // TRR-only
  prefs.setCharPref("network.trr.uri", "https://foo.example.com:" + h2Port + "/dns-auth");
  prefs.setCharPref("network.trr.credentials", "evil:person");
  test_answer = "127.0.0.1";
  listen = dns.asyncResolve("wrong.example.com", 0, listenerFails, mainThread, defaultOriginAttributes);
}

// verify DOH push, part A
function test5()
{
  prefs.setIntPref("network.trr.mode", 3); // TRR-only
  prefs.setCharPref("network.trr.uri", "https://foo.example.com:" + h2Port + "/dns-push");
  test_answer = "127.0.0.1";
  listen = dns.asyncResolve("first.example.com", 0, listenerFine, mainThread, defaultOriginAttributes);
}

function test5b()
{
  // At this point the second host name should've been pushed and we can resolve it using
  // cache only. Set back the URI to a path that fails.
  prefs.setCharPref("network.trr.uri", "https://foo.example.com:" + h2Port + "/404");
  dump("test5b - resolve push.example.now please\n");
  test_answer = "2018::2018";
  listen = dns.asyncResolve("push.example.com", 0, listenerUntilFine, mainThread, defaultOriginAttributes);
}

// verify AAAA entry
function test6()
{
  prefs.setIntPref("network.trr.mode", 3); // TRR-only
  prefs.setCharPref("network.trr.uri", "https://foo.example.com:" + h2Port + "/dns-aaaa");
  test_answer = "2020:2020::2020";
  listen = dns.asyncResolve("aaaa.example.com", 0, listenerFine, mainThread, defaultOriginAttributes);
}

// verify RFC1918 address from the server is rejected
function test7()
{
  prefs.setIntPref("network.trr.mode", 3); // TRR-only
  prefs.setCharPref("network.trr.uri", "https://foo.example.com:" + h2Port + "/dns-rfc1918");
  listen = dns.asyncResolve("rfc1918.example.com", 0, listenerFails, mainThread, defaultOriginAttributes);
}

// verify RFC1918 address from the server is fine when told so
function test8()
{
  prefs.setIntPref("network.trr.mode", 3); // TRR-only
  prefs.setCharPref("network.trr.uri", "https://foo.example.com:" + h2Port + "/dns-rfc1918");
  prefs.setBoolPref("network.trr.allow-rfc1918", true);
  test_answer = "192.168.0.1";
  listen = dns.asyncResolve("rfc1918.example.com", 0, listenerFine, mainThread, defaultOriginAttributes);
}

// use GET
function test9()
{
  prefs.setIntPref("network.trr.mode", 3); // TRR-only
  prefs.setCharPref("network.trr.uri", "https://foo.example.com:" + h2Port + "/dns-get");
  prefs.clearUserPref("network.trr.allow-rfc1918");
  prefs.setBoolPref("network.trr.useGET", true);
  test_answer = "1.2.3.4";
  listen = dns.asyncResolve("get.example.com", 0, listenerFine, mainThread, defaultOriginAttributes);
}

// confirmationNS set without confirmed NS yet
// NOTE: this requires test9 to run before, as the http2 server resets state there
function test10()
{
  prefs.setIntPref("network.trr.mode", 3); // TRR-only
  prefs.clearUserPref("network.trr.useGET");
  prefs.setCharPref("network.trr.uri", "https://foo.example.com:" + h2Port + "/dns-confirm");
  prefs.setCharPref("network.trr.confirmationNS", "confirm.example.com");
  test_loops = 100; // set for test10b
  try {
    listen = dns.asyncResolve("wrong.example.com", 0, listenerFails,
                              mainThread, defaultOriginAttributes);
  } catch (e) {
    // NS_ERROR_UNKNOWN_HOST exception is expected
    do_test_finished();
    do_timeout(200, run_dns_tests);
    //run_dns_tests();
  }
}

// confirmationNS, retry until the confirmed NS works
function test10b()
{
  print("test confirmationNS, retry until the confirmed NS works");
  prefs.setIntPref("network.trr.mode", 3); // TRR-only
  // same URI as in test10
  test_answer = "1::ffff"
  // this test needs to resolve new names in every attempt since the DNS cache
  // will keep the negative resolved info
  try {
    listen = dns.asyncResolve("10b-" + test_loops + ".example.com", 0, listenerUntilFine,
                              mainThread, defaultOriginAttributes);
  } catch(e) {
    // wait a while and try again
    test_loops--;
    do_timeout(200, test10b);
  }
}
// use a slow server and short timeout!
function test11()
{
  prefs.setIntPref("network.trr.mode", 3); // TRR-only
  prefs.setCharPref("network.trr.confirmationNS", "skip");
  prefs.setCharPref("network.trr.uri", "https://foo.example.com:" + h2Port + "/dns-750ms");
  prefs.setIntPref("network.trr.request-timeout", 10);
  listen = dns.asyncResolve("test11.example.com", 0, listenerFails, mainThread, defaultOriginAttributes);
}

// gets an NS back from DOH
function test12()
{
  prefs.setIntPref("network.trr.mode", 2); // TRR-first
  prefs.setCharPref("network.trr.uri", "https://foo.example.com:" + h2Port + "/dns-ns");
  prefs.clearUserPref("network.trr.request-timeout");
  test_answer = "127.0.0.1";
  listen = dns.asyncResolve("test12.example.com", 0, listenerFine, mainThread, defaultOriginAttributes);
}

// TRR-first gets a 404 back from DOH
function test13()
{
  prefs.setIntPref("network.trr.mode", 2); // TRR-first
  prefs.setCharPref("network.trr.uri", "https://foo.example.com:" + h2Port + "/404");
  test_answer = "127.0.0.1";
  listen = dns.asyncResolve("test13.example.com", 0, listenerFine, mainThread, defaultOriginAttributes);
}

// TRR-shadow gets a 404 back from DOH
function test14()
{
  prefs.setIntPref("network.trr.mode", 4); // TRR-shadow
  prefs.setCharPref("network.trr.uri", "https://foo.example.com:" + h2Port + "/404");
  test_answer = "127.0.0.1";
  listen = dns.asyncResolve("test14.example.com", 0, listenerFine, mainThread, defaultOriginAttributes);
}

// TRR-shadow and timed out TRR
function test15()
{
  prefs.setIntPref("network.trr.mode", 4); // TRR-shadow
  prefs.setCharPref("network.trr.uri", "https://foo.example.com:" + h2Port + "/dns-750ms");
  prefs.setIntPref("network.trr.request-timeout", 10);
  test_answer = "127.0.0.1";
  listen = dns.asyncResolve("test15.example.com", 0, listenerFine, mainThread, defaultOriginAttributes);
}

// TRR-first and timed out TRR
function test16()
{
  prefs.setIntPref("network.trr.mode", 2); // TRR-first
  prefs.setCharPref("network.trr.uri", "https://foo.example.com:" + h2Port + "/dns-750ms");
  prefs.setIntPref("network.trr.request-timeout", 10);
  test_answer = "127.0.0.1";
  listen = dns.asyncResolve("test16.example.com", 0, listenerFine, mainThread, defaultOriginAttributes);
}

// TRR-only and chase CNAME
function test17()
{
  prefs.setIntPref("network.trr.mode", 3); // TRR-only
  prefs.setCharPref("network.trr.uri", "https://foo.example.com:" + h2Port + "/dns-cname");
  prefs.clearUserPref("network.trr.request-timeout");
  test_answer = "99.88.77.66";
  listen = dns.asyncResolve("cname.example.com", 0, listenerFine, mainThread, defaultOriginAttributes);
}

// TRR-only and a CNAME loop
function test18()
{
  prefs.setIntPref("network.trr.mode", 3); // TRR-only
  prefs.setCharPref("network.trr.uri", "https://foo.example.com:" + h2Port + "/dns-cname-loop");
  listen = dns.asyncResolve("test18.example.com", 0, listenerFails, mainThread, defaultOriginAttributes);
}

// TRR-race and a CNAME loop
function test19()
{
  prefs.setIntPref("network.trr.mode", 1); // Race them!
  prefs.setCharPref("network.trr.uri", "https://foo.example.com:" + h2Port + "/dns-cname-loop");
  test_answer = "127.0.0.1";
  listen = dns.asyncResolve("test19.example.com", 0, listenerFine, mainThread, defaultOriginAttributes);
}

// TRR-first and a CNAME loop
function test20()
{
  prefs.setIntPref("network.trr.mode", 2); // TRR-first
  prefs.setCharPref("network.trr.uri", "https://foo.example.com:" + h2Port + "/dns-cname-loop");
  test_answer = "127.0.0.1";
  listen = dns.asyncResolve("test20.example.com", 0, listenerFine, mainThread, defaultOriginAttributes);
}

// TRR-shadow and a CNAME loop
function test21()
{
  prefs.setIntPref("network.trr.mode", 4); // shadow
  prefs.setCharPref("network.trr.uri", "https://foo.example.com:" + h2Port + "/dns-cname-loop");
  test_answer = "127.0.0.1";
  listen = dns.asyncResolve("test21.example.com", 0, listenerFine, mainThread, defaultOriginAttributes);
}

// verify that basic A record name mismatch gets rejected. Gets the same DOH
// response back as test1
function test22()
{
  prefs.setIntPref("network.trr.mode", 3); // TRR-only to avoid native fallback
  prefs.setCharPref("network.trr.uri", "https://foo.example.com:" + h2Port + "/dns");
  listen = dns.asyncResolve("mismatch.example.com", 0, listenerFails, mainThread, defaultOriginAttributes);
}

// TRR-only, with a CNAME response with a bundled A record for that CNAME!
function test23()
{
  prefs.setIntPref("network.trr.mode", 3); // TRR-only
  prefs.setCharPref("network.trr.uri", "https://foo.example.com:" + h2Port + "/dns-cname-a");
  test_answer = "9.8.7.6";
  listen = dns.asyncResolve("cname-a.example.com", 0, listenerFine, mainThread, defaultOriginAttributes);
}

var tests = [ test1,
              test1b,
              test2,
              test3,
              test4,
              test5,
              test5b,
              test6,
              test7,
              test8,
              test9,
              test10,
              test10b,
              test11,
              test12,
              test13,
              test14,
              test15,
              test16,
              test17,
              test18,
              test19,
              test20,
              test21,
              test22,
              test23,
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
