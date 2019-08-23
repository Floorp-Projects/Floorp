"use strict";

const dns = Cc["@mozilla.org/network/dns-service;1"].getService(
  Ci.nsIDNSService
);
const mainThread = Services.tm.currentThread;

const defaultOriginAttributes = {};
let h2Port = null;

add_task(function setup() {
  dump("start!\n");

  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
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
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh`
  );
  // make all native resolve calls "secretly" resolve localhost instead
  Services.prefs.setBoolPref("network.dns.native-is-localhost", true);

  // 0 - off, 1 - reserved, 2 - TRR first, 3  - TRR only, 4 - reserved
  Services.prefs.setIntPref("network.trr.mode", 2); // TRR first
  Services.prefs.setBoolPref("network.trr.wait-for-portal", false);
  // By default wait for all responses before notifying the listeners.
  Services.prefs.setBoolPref("network.trr.wait-for-A-and-AAAA", true);
  // don't confirm that TRR is working, just go!
  Services.prefs.setCharPref("network.trr.confirmationNS", "skip");

  // The moz-http2 cert is for foo.example.com and is signed by http2-ca.pem
  // so add that cert to the trust list as a signing cert.  // the foo.example.com domain name.
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
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
  Services.prefs.clearUserPref("network.trr.early-AAAA");
  Services.prefs.clearUserPref("network.trr.skip-AAAA-when-not-supported");
  Services.prefs.clearUserPref("network.trr.wait-for-A-and-AAAA");
  Services.prefs.clearUserPref("network.trr.excluded-domains");
  Services.prefs.clearUserPref("captivedetect.canonicalURL");

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
    this.promise = new Promise(resolve => {
      this.resolve = resolve;
    });
    this.request = dns.asyncResolve(
      name,
      0,
      this,
      mainThread,
      defaultOriginAttributes
    );
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
    if (aIID.equals(Ci.nsIDNSListener) || aIID.equals(Ci.nsISupports)) {
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
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=2.2.2.2`
  );

  await new DNSListener("bar.example.com", "2.2.2.2");
});

// verify basic A record - without bootstrapping
add_task(async function test1b() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=3.3.3.3`
  );
  Services.prefs.clearUserPref("network.trr.bootstrapAddress");
  Services.prefs.setCharPref("network.dns.localDomains", "foo.example.com");

  await new DNSListener("bar.example.com", "3.3.3.3");

  Services.prefs.setCharPref("network.trr.bootstrapAddress", "127.0.0.1");
  Services.prefs.clearUserPref("network.dns.localDomains");
});

// verify that the name was put in cache - it works with bad DNS URI
add_task(async function test2() {
  // Don't clear the cache. That is what we're checking.
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/404`
  );

  await new DNSListener("bar.example.com", "3.3.3.3");
});

// verify working credentials in DOH request
add_task(async function test3() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=4.4.4.4&auth=true`
  );
  Services.prefs.setCharPref("network.trr.credentials", "user:password");

  await new DNSListener("bar.example.com", "4.4.4.4");
});

// verify failing credentials in DOH request
add_task(async function test4() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=4.4.4.4&auth=true`
  );
  Services.prefs.setCharPref("network.trr.credentials", "evil:person");

  let [, , inStatus] = await new DNSListener(
    "wrong.example.com",
    undefined,
    false
  );
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
});

// verify DOH push, part A
add_task(async function test5() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=5.5.5.5&push=true`
  );

  await new DNSListener("first.example.com", "5.5.5.5");
});

add_task(async function test5b() {
  // At this point the second host name should've been pushed and we can resolve it using
  // cache only. Set back the URI to a path that fails.
  // Don't clear the cache, otherwise we lose the pushed record.
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/404`
  );
  dump("test5b - resolve push.example.now please\n");

  await new DNSListener("push.example.com", "2018::2018");
});

// verify AAAA entry
add_task(async function test6() {
  Services.prefs.setBoolPref("network.trr.wait-for-A-and-AAAA", true);

  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.early-AAAA", true); // ignored when wait-for-A-and-AAAA is true
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=2020:2020::2020&delayIPv4=100`
  );
  await new DNSListener("aaaa.example.com", "2020:2020::2020");

  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.early-AAAA", false); // ignored when wait-for-A-and-AAAA is true
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=2020:2020::2030&delayIPv4=100`
  );
  await new DNSListener("aaaa.example.com", "2020:2020::2030");

  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.early-AAAA", true); // ignored when wait-for-A-and-AAAA is true
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=2020:2020::2020&delayIPv6=100`
  );
  await new DNSListener("aaaa.example.com", "2020:2020::2020");

  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.early-AAAA", false); // ignored when wait-for-A-and-AAAA is true
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=2020:2020::2030&delayIPv6=100`
  );
  await new DNSListener("aaaa.example.com", "2020:2020::2030");

  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.early-AAAA", true); // ignored when wait-for-A-and-AAAA is true
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=2020:2020::2020`
  );
  await new DNSListener("aaaa.example.com", "2020:2020::2020");

  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.early-AAAA", false); // ignored when wait-for-A-and-AAAA is true
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=2020:2020::2030`
  );
  await new DNSListener("aaaa.example.com", "2020:2020::2030");

  Services.prefs.setBoolPref("network.trr.wait-for-A-and-AAAA", false);

  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.early-AAAA", true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=2020:2020::2020&delayIPv4=100`
  );
  await new DNSListener("aaaa.example.com", "2020:2020::2020");

  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.early-AAAA", false);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=2020:2020::2030&delayIPv4=100`
  );
  await new DNSListener("aaaa.example.com", "2020:2020::2030");

  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.early-AAAA", true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=2020:2020::2020&delayIPv6=100`
  );
  await new DNSListener("aaaa.example.com", "2020:2020::2020");

  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.early-AAAA", false);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=2020:2020::2030&delayIPv6=100`
  );
  await new DNSListener("aaaa.example.com", "2020:2020::2030");

  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.early-AAAA", true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=2020:2020::2020`
  );
  await new DNSListener("aaaa.example.com", "2020:2020::2020");

  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.early-AAAA", false);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=2020:2020::2030`
  );
  await new DNSListener("aaaa.example.com", "2020:2020::2030");

  Services.prefs.clearUserPref("network.trr.early-AAAA");
  Services.prefs.clearUserPref("network.trr.wait-for-A-and-AAAA");
});

// verify RFC1918 address from the server is rejected
add_task(async function test7() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=192.168.0.1`
  );
  let [, , inStatus] = await new DNSListener(
    "rfc1918.example.com",
    undefined,
    false
  );
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
});

// verify RFC1918 address from the server is fine when told so
add_task(async function test8() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=192.168.0.1`
  );
  Services.prefs.setBoolPref("network.trr.allow-rfc1918", true);
  await new DNSListener("rfc1918.example.com", "192.168.0.1");
});

// use GET and disable ECS (makes a larger request)
// verify URI template cutoff
add_task(async function test8b() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh{?dns}`
  );
  Services.prefs.clearUserPref("network.trr.allow-rfc1918");
  Services.prefs.setBoolPref("network.trr.useGET", true);
  Services.prefs.setBoolPref("network.trr.disable-ECS", true);
  await new DNSListener("ecs.example.com", "5.5.5.5");
});

// use GET
add_task(async function test9() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh`
  );
  Services.prefs.clearUserPref("network.trr.allow-rfc1918");
  Services.prefs.setBoolPref("network.trr.useGET", true);
  Services.prefs.setBoolPref("network.trr.disable-ECS", false);
  await new DNSListener("get.example.com", "5.5.5.5");
});

// confirmationNS set without confirmed NS yet
add_task(async function test10() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.clearUserPref("network.trr.useGET");
  Services.prefs.clearUserPref("network.trr.disable-ECS");
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=1::ffff`
  );
  Services.prefs.setCharPref(
    "network.trr.confirmationNS",
    "confirm.example.com"
  );

  try {
    let [, , inStatus] = await new DNSListener(
      "wrong.example.com",
      undefined,
      false
    );
    Assert.ok(
      !Components.isSuccessCode(inStatus),
      `${inStatus} should be an error code`
    );
  } catch (e) {
    await new Promise(resolve => do_timeout(200, resolve));
  }

  // confirmationNS, retry until the confirmed NS works
  let test_loops = 100;
  print("test confirmationNS, retry until the confirmed NS works");

  // this test needs to resolve new names in every attempt since the DNS
  // will keep the negative resolved info
  while (test_loops > 0) {
    print(`loops remaining: ${test_loops}\n`);
    try {
      let [, inRecord, inStatus] = await new DNSListener(
        `10b-${test_loops}.example.com`,
        undefined,
        false
      );
      if (inRecord) {
        Assert.equal(inStatus, Cr.NS_OK);
        let answer = inRecord.getNextAddrAsString();
        Assert.equal(answer, "1::ffff");
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
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/dns-750ms`
  );
  Services.prefs.setIntPref("network.trr.request-timeout", 10);
  let [, , inStatus] = await new DNSListener(
    "test11.example.com",
    undefined,
    false
  );
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
});

// gets an no answers back from DoH. Falls back to regular DNS
add_task(async function test12() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 2); // TRR-first
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=none`
  );
  Services.prefs.clearUserPref("network.trr.request-timeout");
  await new DNSListener("confirm.example.com", "127.0.0.1");
});

// TRR-first gets a 404 back from DOH
add_task(async function test13() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 2); // TRR-first
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/404`
  );
  await new DNSListener("test13.example.com", "127.0.0.1");
});

// Test that MODE_RESERVED4 (previously MODE_SHADOW) is treated as TRR off.
add_task(async function test14() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 4); // MODE_RESERVED4. Interpreted as TRR off.
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/404`
  );
  await new DNSListener("test14.example.com", "127.0.0.1");

  Services.prefs.setIntPref("network.trr.mode", 4); // MODE_RESERVED4. Interpreted as TRR off.
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=2.2.2.2`
  );
  await new DNSListener("bar.example.com", "127.0.0.1");
});

// Test that MODE_RESERVED4 (previously MODE_SHADOW) is treated as TRR off.
add_task(async function test15() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 4); // MODE_RESERVED4. Interpreted as TRR off.
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/dns-750ms`
  );
  Services.prefs.setIntPref("network.trr.request-timeout", 10);
  await new DNSListener("test15.example.com", "127.0.0.1");

  Services.prefs.setIntPref("network.trr.mode", 4); // MODE_RESERVED4. Interpreted as TRR off.
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=2.2.2.2`
  );
  await new DNSListener("bar.example.com", "127.0.0.1");
});

// TRR-first and timed out TRR
add_task(async function test16() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 2); // TRR-first
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/dns-750ms`
  );
  Services.prefs.setIntPref("network.trr.request-timeout", 10);
  await new DNSListener("test16.example.com", "127.0.0.1");
});

// TRR-only and chase CNAME
add_task(async function test17() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/dns-cname`
  );
  Services.prefs.clearUserPref("network.trr.request-timeout");
  await new DNSListener("cname.example.com", "99.88.77.66");
});

// TRR-only and a CNAME loop
add_task(async function test18() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=none&cnameloop=true`
  );
  let [, , inStatus] = await new DNSListener(
    "test18.example.com",
    undefined,
    false
  );
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
});

// Test that MODE_RESERVED1 (previously MODE_PARALLEL) is treated as TRR off.
add_task(async function test19() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 1); // MODE_RESERVED1. Interpreted as TRR off.
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=none&cnameloop=true`
  );
  await new DNSListener("test19.example.com", "127.0.0.1");

  Services.prefs.setIntPref("network.trr.mode", 1); // MODE_RESERVED1. Interpreted as TRR off.
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=2.2.2.2`
  );
  await new DNSListener("bar.example.com", "127.0.0.1");
});

// TRR-first and a CNAME loop
add_task(async function test20() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 2); // TRR-first
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=none&cnameloop=true`
  );
  await new DNSListener("test20.example.com", "127.0.0.1");
});

// Test that MODE_RESERVED4 (previously MODE_SHADOW) is treated as TRR off.
add_task(async function test21() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 4); // MODE_RESERVED4. Interpreted as TRR off.
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=none&cnameloop=true`
  );
  await new DNSListener("test21.example.com", "127.0.0.1");

  Services.prefs.setIntPref("network.trr.mode", 4); // MODE_RESERVED4. Interpreted as TRR off.
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=2.2.2.2`
  );
  await new DNSListener("bar.example.com", "127.0.0.1");
});

// verify that basic A record name mismatch gets rejected.
// Gets a response for bar.example.com instead of what it requested
add_task(async function test22() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only to avoid native fallback
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?hostname=bar.example.com`
  );
  let [, , inStatus] = await new DNSListener(
    "mismatch.example.com",
    undefined,
    false
  );
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
});

// TRR-only, with a CNAME response with a bundled A record for that CNAME!
add_task(async function test23() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/dns-cname-a`
  );
  await new DNSListener("cname-a.example.com", "9.8.7.6");
});

// TRR-first check that TRR result is used
add_task(async function test24() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 2); // TRR-first
  Services.prefs.setCharPref("network.trr.excluded-domains", "");
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=192.192.192.192`
  );
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

// TRR-first check that DNS result is used if domain is part of the excluded-domains pref that contains things before it.
add_task(async function test24d() {
  dns.clearCache(true);
  Services.prefs.setCharPref(
    "network.trr.excluded-domains",
    "foo.test.com, bar.example.com"
  );
  await new DNSListener("bar.example.com", "127.0.0.1");
});

// TRR-first check that DNS result is used if domain is part of the excluded-domains pref that contains things after it.
add_task(async function test24e() {
  dns.clearCache(true);
  Services.prefs.setCharPref(
    "network.trr.excluded-domains",
    "bar.example.com, foo.test.com"
  );
  await new DNSListener("bar.example.com", "127.0.0.1");
});

// TRR-first check that captivedetect.canonicalURL is resolved via native DNS
add_task(async function test24f() {
  dns.clearCache(true);
  Services.prefs.setCharPref(
    "captivedetect.canonicalURL",
    "http://test.detectportal.com/success.txt"
  );

  await new DNSListener("test.detectportal.com", "127.0.0.1");
});

// TRR-only that resolving localhost with TRR-only mode will use the remote
// resolver if it's not in the excluded domains
add_task(async function test25() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.excluded-domains", "");
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=192.192.192.192`
  );

  await new DNSListener("localhost", "192.192.192.192", true);
});

// TRR-only check that localhost goes directly to native lookup when in the excluded-domains
add_task(async function test25b() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.excluded-domains", "localhost");
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=192.192.192.192`
  );

  await new DNSListener("localhost", "127.0.0.1");
});

// TRR-only check that test.local is resolved via native DNS
add_task(async function test25c() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.excluded-domains", "localhost,local");
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=192.192.192.192`
  );

  await new DNSListener("test.local", "127.0.0.1");
});

// TRR-only check that .other is resolved via native DNS when the pref is set
add_task(async function test25d() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.excluded-domains",
    "localhost,local,other"
  );
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=192.192.192.192`
  );

  await new DNSListener("domain.other", "127.0.0.1");
});

// TRR-only check that captivedetect.canonicalURL is resolved via native DNS
add_task(async function test25e() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "captivedetect.canonicalURL",
    "http://test.detectportal.com/success.txt"
  );
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=192.192.192.192`
  );

  await new DNSListener("test.detectportal.com", "127.0.0.1");
});

// Check that none of the requests have set any cookies.
add_task(async function count_cookies() {
  let cm = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager);
  Assert.equal(cm.countCookiesFromHost("example.com"), 0);
  Assert.equal(cm.countCookiesFromHost("foo.example.com."), 0);
});

add_task(async function test_connection_closed() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.excluded-domains", "");
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=2.2.2.2`
  );
  // bootstrap
  Services.prefs.clearUserPref("network.dns.localDomains");
  Services.prefs.setCharPref("network.trr.bootstrapAddress", "127.0.0.1");

  await new DNSListener("bar.example.com", "2.2.2.2");

  // makes the TRR connection shut down.
  let [, , inStatus] = await new DNSListener("closeme.com", undefined, false);
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
  await new DNSListener("bar2.example.com", "2.2.2.2");
});

add_task(async function test_connection_closed_no_bootstrap() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.excluded-domains", "localhost,local");
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=3.3.3.3`
  );
  Services.prefs.setCharPref("network.dns.localDomains", "foo.example.com");
  Services.prefs.clearUserPref("network.trr.bootstrapAddress");

  await new DNSListener("bar.example.com", "3.3.3.3");

  // makes the TRR connection shut down.
  let [, , inStatus] = await new DNSListener("closeme.com", undefined, false);
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
  await new DNSListener("bar2.example.com", "3.3.3.3");
});

add_task(async function test_connection_closed_no_bootstrap_localhost() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.excluded-domains", "localhost");
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://localhost:${h2Port}/doh?responseIP=3.3.3.3`
  );
  Services.prefs.clearUserPref("network.dns.localDomains");
  Services.prefs.clearUserPref("network.trr.bootstrapAddress");

  await new DNSListener("bar.example.com", "3.3.3.3");

  // makes the TRR connection shut down.
  let [, , inStatus] = await new DNSListener("closeme.com", undefined, false);
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
  await new DNSListener("bar2.example.com", "3.3.3.3");
});

add_task(async function test_connection_closed_no_bootstrap_no_excluded() {
  // This test exists to document what happens when we're in TRR only mode
  // and we don't set a bootstrap address. We use DNS to resolve the
  // initial URI, but if the connection fails, we don't fallback to DNS
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.excluded-domains", "");
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://localhost:${h2Port}/doh?responseIP=3.3.3.3`
  );
  Services.prefs.clearUserPref("network.dns.localDomains");
  Services.prefs.clearUserPref("network.trr.bootstrapAddress");

  await new DNSListener("bar.example.com", "3.3.3.3");

  // makes the TRR connection shut down.
  let [, , inStatus] = await new DNSListener("closeme.com", undefined, false);
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
  [, , inStatus] = await new DNSListener("bar2.example.com", undefined, false);
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
});

add_task(async function test_connection_closed_trr_first() {
  // This test exists to document what happens when we're in TRR only mode
  // and we don't set a bootstrap address. We use DNS to resolve the
  // initial URI, but if the connection fails, we don't fallback to DNS
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 2); // TRR-first
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://localhost:${h2Port}/doh?responseIP=9.9.9.9`
  );
  Services.prefs.setCharPref("network.dns.localDomains", "closeme.com");
  Services.prefs.clearUserPref("network.trr.bootstrapAddress");

  await new DNSListener("bar.example.com", "9.9.9.9");

  // makes the TRR connection shut down. Should fallback to DNS
  await new DNSListener("closeme.com", "127.0.0.1");
  // TRR should be back up again
  await new DNSListener("bar2.example.com", "9.9.9.9");
});
