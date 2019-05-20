"use strict";


const dns = Cc["@mozilla.org/network/dns-service;1"].getService(Ci.nsIDNSService);
const mainThread = Services.tm.currentThread;

const defaultOriginAttributes = {};
let h2Port = null;

add_task(function setup() {
  dump("start!\n");

  let env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  h2Port = env.get("MOZHTTP2_PORT");
  Assert.notEqual(h2Port, null);
  Assert.notEqual(h2Port, "");

  // Set to allow the cert presented by our H2 server
  do_get_profile();

  Services.prefs.setBoolPref("network.http.spdy.enabled", true);
  Services.prefs.setBoolPref("network.http.spdy.enabled.http2", true);
  // the TRR server is on 127.0.0.1
  Services.prefs.setCharPref("network.trr.bootstrapAddress", "127.0.0.1");

  // use the h2 server as DOH provider
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/dns`);
  // make all native resolve calls "secretly" resolve localhost instead
  Services.prefs.setBoolPref("network.dns.native-is-localhost", true);

  // 0 - off, 1 - race, 2 TRR first, 3 TRR only, 4 shadow
  Services.prefs.setIntPref("network.trr.mode", 2); // TRR first
  Services.prefs.setBoolPref("network.trr.wait-for-portal", false);
  // don't confirm that TRR is working, just go!
  Services.prefs.setCharPref("network.trr.confirmationNS", "skip");

  // The moz-http2 cert is for foo.example.com and is signed by http2-ca.pem
  // so add that cert to the trust list as a signing cert.  // the foo.example.com domain name.
  let certdb = Cc["@mozilla.org/security/x509certdb;1"]
      .getService(Ci.nsIX509CertDB);
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");
});

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("network.trr.mode");
  Services.prefs.clearUserPref("network.trr.uri");
  Services.prefs.clearUserPref("network.trr.credentials");
  Services.prefs.clearUserPref("network.trr.wait-for-portal");
  Services.prefs.clearUserPref("network.trr.allow-rfc1918");
  Services.prefs.clearUserPref("network.trr.useGET");
  Services.prefs.clearUserPref("network.trr.confirmationNS");
  Services.prefs.clearUserPref("network.trr.bootstrapAddress");
  Services.prefs.clearUserPref("network.trr.blacklist-duration");
  Services.prefs.clearUserPref("network.trr.request-timeout");
  Services.prefs.clearUserPref("network.trr.disable-ECS");
  Services.prefs.clearUserPref("network.trr.excluded-domains");

  Services.prefs.clearUserPref("network.http.spdy.enabled");
  Services.prefs.clearUserPref("network.http.spdy.enabled.http2");
  Services.prefs.clearUserPref("network.dns.localDomains");
  Services.prefs.clearUserPref("network.dns.native-is-localhost");
});

class DNSListener {
  constructor(name, expectedAnswer, expectedSuccess = true) {
    this.name = name;
    this.expectedAnswer = expectedAnswer;
    this.expectedSuccess = expectedSuccess;
    this.promise = new Promise(resolve => { this.resolve = resolve; });
    this.request = dns.asyncResolve(name, 0, this, mainThread, defaultOriginAttributes);
  }

  onLookupComplete(inRequest, inRecord, inStatus) {
    Assert.ok(inRequest == this.request);

    // If we don't expect success here, just resolve and the caller will
    // decide what to do with the results.
    if (!this.expectedSuccess) {
      this.resolve([inRequest, inRecord, inStatus]);
      return;
    }

    Assert.equal(inStatus, Cr.NS_OK);
    let answer = inRecord.getNextAddrAsString();
    Assert.equal(answer, this.expectedAnswer);
    this.resolve([inRequest, inRecord, inStatus]);
  }

  QueryInterface(aIID) {
    if (aIID.equals(Ci.nsIDNSListener) ||
        aIID.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }

  // Implement then so we can await this as a promise.
  then() {
    return this.promise.then.apply(this.promise, arguments);
  }
}


// verify basic A record
add_task(async function test1() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 2); // TRR-first
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/doh?responseIP=2.2.2.2`);

  await new DNSListener("bar.example.com", "2.2.2.2");
});

// verify basic A record - without bootstrapping
add_task(async function test1b() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/doh?responseIP=3.3.3.3`);
  Services.prefs.clearUserPref("network.trr.bootstrapAddress");
  Services.prefs.setCharPref("network.dns.localDomains", "foo.example.com");

  await new DNSListener("bar.example.com", "3.3.3.3");
});

// verify that the name was put in cache - it works with bad DNS URI
add_task(async function test2() {
  // Don't clear the cache. That is what we're checking.
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/404`);

  await new DNSListener("bar.example.com", "3.3.3.3");
});

// verify working credentials in DOH request
add_task(async function test3() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/doh?responseIP=4.4.4.4&auth=true`);
  Services.prefs.setCharPref("network.trr.credentials", "user:password");

  await new DNSListener("bar.example.com", "4.4.4.4");
});

// verify failing credentials in DOH request
add_task(async function test4() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/doh?responseIP=4.4.4.4&auth=true`);
  Services.prefs.setCharPref("network.trr.credentials", "evil:person");

  let [, , inStatus] = await new DNSListener("wrong.example.com", undefined, false);
  Assert.ok(!Components.isSuccessCode(inStatus), `${inStatus} should be an error code`);
});

// verify DOH push, part A
add_task(async function test5() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/doh?responseIP=5.5.5.5&push=true`);

  await new DNSListener("first.example.com", "5.5.5.5");
});

add_task(async function test5b() {
  // At this point the second host name should've been pushed and we can resolve it using
  // cache only. Set back the URI to a path that fails.
  // Don't clear the cache, otherwise we lose the pushed record.
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/404`);
  dump("test5b - resolve push.example.now please\n");

  await new DNSListener("push.example.com", "2018::2018");
});

// verify AAAA entry
add_task(async function test6() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/doh?responseIP=2020:2020::2020`);
  await new DNSListener("aaaa.example.com", "2020:2020::2020");
});

// verify RFC1918 address from the server is rejected
add_task(async function test7() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/doh?responseIP=192.168.0.1`);
  let [, , inStatus] = await new DNSListener("rfc1918.example.com", undefined, false);
  Assert.ok(!Components.isSuccessCode(inStatus), `${inStatus} should be an error code`);
});

// verify RFC1918 address from the server is fine when told so
add_task(async function test8() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/doh?responseIP=192.168.0.1`);
  Services.prefs.setBoolPref("network.trr.allow-rfc1918", true);
  await new DNSListener("rfc1918.example.com", "192.168.0.1");
});

// use GET and disable ECS (makes a larger request)
// verify URI template cutoff
add_task(async function test8b() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/doh{?dns}`);
  Services.prefs.clearUserPref("network.trr.allow-rfc1918");
  Services.prefs.setBoolPref("network.trr.useGET", true);
  Services.prefs.setBoolPref("network.trr.disable-ECS", true);
  await new DNSListener("ecs.example.com", "5.5.5.5");
});

// use GET
add_task(async function test9() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/doh`);
  Services.prefs.clearUserPref("network.trr.allow-rfc1918");
  Services.prefs.setBoolPref("network.trr.useGET", true);
  Services.prefs.setBoolPref("network.trr.disable-ECS", false);
  await new DNSListener("get.example.com", "5.5.5.5");
});

// confirmationNS set without confirmed NS yet
// NOTE: this requires test9 to run before, as the http2 server resets state there
add_task(async function test10() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.clearUserPref("network.trr.useGET");
  Services.prefs.clearUserPref("network.trr.disable-ECS");
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/dns-confirm`);
  Services.prefs.setCharPref("network.trr.confirmationNS", "confirm.example.com");

  try {
    let [, , inStatus] = await new DNSListener("wrong.example.com", undefined, false);
    Assert.ok(!Components.isSuccessCode(inStatus), `${inStatus} should be an error code`);
  } catch (e) {
    await new Promise(resolve => do_timeout(200, resolve));
  }
});

// confirmationNS, retry until the confirmed NS works
add_task(async function test10b() {
  dns.clearCache(true);
  let test_loops = 100;
  print("test confirmationNS, retry until the confirmed NS works");
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  // same URI as in test10

  // this test needs to resolve new names in every attempt since the DNS
  // will keep the negative resolved info
  while (test_loops > 0) {
    print(`loops remaining: ${test_loops}`);
    try {
      let [, inRecord, inStatus] = await new DNSListener(`10b-${test_loops}.example.com`, undefined, false);
      if (inRecord) {
        Assert.equal(inStatus, Cr.NS_OK);
        break;
      }
    } catch (e) {
      dump(e);
    }

    test_loops--;
    await new Promise(resolve => do_timeout(0, resolve));
  }
  Assert.notEqual(test_loops, 0);
});

// use a slow server and short timeout!
add_task(async function test11() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.confirmationNS", "skip");
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/dns-750ms`);
  Services.prefs.setIntPref("network.trr.request-timeout", 10);
  let [, , inStatus] = await new DNSListener("test11.example.com", undefined, false);
  Assert.ok(!Components.isSuccessCode(inStatus), `${inStatus} should be an error code`);
});

// gets an NS back from DOH
add_task(async function test12() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 2); // TRR-first
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/dns-ns`);
  Services.prefs.clearUserPref("network.trr.request-timeout");
  await new DNSListener("test12.example.com", "127.0.0.1");
});

// TRR-first gets a 404 back from DOH
add_task(async function test13() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 2); // TRR-first
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/404`);
  await new DNSListener("test13.example.com", "127.0.0.1");
});

// TRR-shadow gets a 404 back from DOH
add_task(async function test14() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 4); // TRR-shadow
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/404`);
  await new DNSListener("test14.example.com", "127.0.0.1");
});

// TRR-shadow and timed out TRR
add_task(async function test15() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 4); // TRR-shadow
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/dns-750ms`);
  Services.prefs.setIntPref("network.trr.request-timeout", 10);
  await new DNSListener("test15.example.com", "127.0.0.1");
});

// TRR-first and timed out TRR
add_task(async function test16() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 2); // TRR-first
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/dns-750ms`);
  Services.prefs.setIntPref("network.trr.request-timeout", 10);
  await new DNSListener("test16.example.com", "127.0.0.1");
});

// TRR-only and chase CNAME
add_task(async function test17() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/dns-cname`);
  Services.prefs.clearUserPref("network.trr.request-timeout");
  await new DNSListener("cname.example.com", "99.88.77.66");
});

// TRR-only and a CNAME loop
add_task(async function test18() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/dns-cname-loop`);
  let [, , inStatus] = await new DNSListener("test18.example.com", undefined, false);
  Assert.ok(!Components.isSuccessCode(inStatus), `${inStatus} should be an error code`);
});

// TRR-race and a CNAME loop
add_task(async function test19() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 1); // Race them!
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/dns-cname-loop`);
  await new DNSListener("test19.example.com", "127.0.0.1");
});

// TRR-first and a CNAME loop
add_task(async function test20() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 2); // TRR-first
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/dns-cname-loop`);
  await new DNSListener("test20.example.com", "127.0.0.1");
});

// TRR-shadow and a CNAME loop
add_task(async function test21() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 4); // shadow
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/dns-cname-loop`);
  await new DNSListener("test21.example.com", "127.0.0.1");
});

// verify that basic A record name mismatch gets rejected.
// Gets a response for bar.example.com instead of what it requested
add_task(async function test22() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only to avoid native fallback
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/dns`);
  let [, , inStatus] = await new DNSListener("mismatch.example.com", undefined, false);
  Assert.ok(!Components.isSuccessCode(inStatus), `${inStatus} should be an error code`);
});

// TRR-only, with a CNAME response with a bundled A record for that CNAME!
add_task(async function test23() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/dns-cname-a`);
  await new DNSListener("cname-a.example.com", "9.8.7.6");
});

// TRR-first check that TRR result is used
add_task(async function test24() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 2); // TRR-first
  Services.prefs.setCharPref("network.trr.excluded-domains", "");
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/doh?responseIP=192.192.192.192`);
  await new DNSListener("bar.example.com", "192.192.192.192");
});

// TRR-first check that DNS result is used if domain is part of the excluded-domains pref
add_task(async function test24b() {
  dns.clearCache(true);
  Services.prefs.setCharPref("network.trr.excluded-domains", "bar.example.com");
  await new DNSListener("bar.example.com", "127.0.0.1");
});

// TRR-first check that DNS result is used if domain is part of the excluded-domains pref
add_task(async function test24c() {
  dns.clearCache(true);
  Services.prefs.setCharPref("network.trr.excluded-domains", "example.com");
  await new DNSListener("bar.example.com", "127.0.0.1");
});

// TRR-only that resolving localhost with TRR-only mode will use the remote
// resolver if it's not in the excluded domains
add_task(async function test25() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.excluded-domains", "");
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/doh?responseIP=192.192.192.192`);

  await new DNSListener("localhost", "192.192.192.192", true);
});

// TRR-only check that localhost goes directly to native lookup when in the excluded-domains
add_task(async function test25b() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.excluded-domains", "localhost");
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/doh?responseIP=192.192.192.192`);

  await new DNSListener("localhost", "127.0.0.1");
});

// TRR-only check that test.local is resolved via native DNS
add_task(async function test25c() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.excluded-domains", "localhost,local");
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/doh?responseIP=192.192.192.192`);

  await new DNSListener("test.local", "127.0.0.1");
});

// TRR-only check that .other is resolved via native DNS when the pref is set
add_task(async function test25d() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.excluded-domains", "localhost,local,other");
  Services.prefs.setCharPref("network.trr.uri", `https://foo.example.com:${h2Port}/doh?responseIP=192.192.192.192`);

  await new DNSListener("domain.other", "127.0.0.1");
});
