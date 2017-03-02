/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");

Cu.import("chrome://marionette/content/error.js");
Cu.import("chrome://marionette/content/session.js");

add_test(function test_Timeouts_ctor() {
  let ts = new session.Timeouts();
  equal(ts.implicit, 0);
  equal(ts.pageLoad, 300000);
  equal(ts.script, 30000);

  run_next_test();
});

add_test(function test_Timeouts_toString() {
  equal(new session.Timeouts().toString(), "[object session.Timeouts]");

  run_next_test();
});

add_test(function test_Timeouts_toJSON() {
  let ts = new session.Timeouts();
  deepEqual(ts.toJSON(), {"implicit": 0, "page load": 300000, "script": 30000});

  run_next_test();
});

add_test(function test_Timeouts_fromJSON() {
  let json = {
    "implicit": 10,
    "page load": 20,
    "script": 30,
  };
  let ts = session.Timeouts.fromJSON(json);
  equal(ts.implicit, json["implicit"]);
  equal(ts.pageLoad, json["page load"]);
  equal(ts.script, json["script"]);

  run_next_test();
});

add_test(function test_PageLoadStrategy() {
  equal(session.PageLoadStrategy.None, "none");
  equal(session.PageLoadStrategy.Eager, "eager");
  equal(session.PageLoadStrategy.Normal, "normal");

  run_next_test();
});

add_test(function test_Proxy_ctor() {
  let p = new session.Proxy();
  let props = [
    "proxyType",
    "httpProxy",
    "httpProxyPort",
    "sslProxy",
    "sslProxyPort",
    "ftpProxy",
    "ftpProxyPort",
    "socksProxy",
    "socksProxyPort",
    "socksVersion",
    "proxyAutoconfigUrl",
  ];
  for (let prop of props) {
    ok(prop in p, `${prop} in ${JSON.stringify(props)}`);
    equal(p[prop], null);
  }

  run_next_test();
});

add_test(function test_Proxy_init() {
  let p = new session.Proxy();

  // no changed made, and 5 (system) is default
  equal(p.init(), false);
  equal(Preferences.get("network.proxy.type"), 5);

  // pac
  p.proxyType = "pac";
  p.proxyAutoconfigUrl = "http://localhost:1234";
  ok(p.init());

  equal(Preferences.get("network.proxy.type"), 2);
  equal(Preferences.get("network.proxy.autoconfig_url"),
      "http://localhost:1234");

  // autodetect
  p = new session.Proxy();
  p.proxyType = "autodetect";
  ok(p.init());
  equal(Preferences.get("network.proxy.type"), 4);

  // system
  p = new session.Proxy();
  p.proxyType = "system";
  ok(p.init());
  equal(Preferences.get("network.proxy.type"), 5);

  // noproxy
  p = new session.Proxy();
  p.proxyType = "noproxy";
  ok(p.init());
  equal(Preferences.get("network.proxy.type"), 0);

  run_next_test();
});

add_test(function test_Proxy_toString() {
  equal(new session.Proxy().toString(), "[object session.Proxy]");

  run_next_test();
});

add_test(function test_Proxy_toJSON() {
  let p = new session.Proxy();
  deepEqual(p.toJSON(), {});

  p = new session.Proxy();
  p.proxyType = "manual";
  deepEqual(p.toJSON(), {proxyType: "manual"});

  run_next_test();
});

add_test(function test_Proxy_fromJSON() {
  deepEqual({}, session.Proxy.fromJSON(undefined).toJSON());
  deepEqual({}, session.Proxy.fromJSON(null).toJSON());

  for (let typ of [true, 42, "foo", []]) {
    Assert.throws(() => session.Proxy.fromJSON(typ), InvalidArgumentError);
  }

  // must contain proxyType
  Assert.throws(() => session.Proxy.fromJSON({}), InvalidArgumentError);
  deepEqual({proxyType: "foo"},
      session.Proxy.fromJSON({proxyType: "foo"}).toJSON());

  // manual
  session.Proxy.fromJSON({proxyType: "manual"});

  for (let proxy of ["httpProxy", "sslProxy", "ftpProxy", "socksProxy"]) {
    let manual = {proxyType: "manual"};

    for (let typ of [true, 42, [], {}, null]) {
      manual[proxy] = typ;
      Assert.throws(() => session.Proxy.fromJSON(manual),
          InvalidArgumentError);
    }

    manual[proxy] = "foo";
    Assert.throws(() => session.Proxy.fromJSON(manual),
        InvalidArgumentError);

    for (let typ of ["bar", true, [], {}, null, undefined]) {
      manual[proxy + "Port"] = typ;
      Assert.throws(() => session.Proxy.fromJSON(manual),
          InvalidArgumentError);
    }

    manual[proxy] = "foo";
    manual[proxy + "Port"] = 1234;

    let expected = {
      "proxyType": "manual",
      [proxy]: "foo",
      [proxy + "Port"]: 1234,
    };

    if (proxy == "socksProxy") {
      manual.socksProxyVersion = 42;
      expected.socksProxyVersion = 42;
    }
    deepEqual(expected, session.Proxy.fromJSON(manual).toJSON());
  }

  Assert.throws(() => session.Proxy.fromJSON(
      {proxyType: "manual", socksProxy: "foo", socksProxyPort: 1234}),
      InvalidArgumentError);

  run_next_test();
});

add_test(function test_Capabilities_ctor() {
  let caps = new session.Capabilities();
  ok(caps.has("browserName"));
  ok(caps.has("browserVersion"));
  ok(caps.has("platformName"));
  ok(caps.has("platformVersion"));
  equal(session.PageLoadStrategy.Normal, caps.get("pageLoadStrategy"));
  equal(false, caps.get("acceptInsecureCerts"));
  ok(caps.get("timeouts") instanceof session.Timeouts);
  ok(caps.get("proxy") instanceof session.Proxy);

  ok(caps.has("rotatable"));

  equal(0, caps.get("specificationLevel"));
  ok(caps.has("moz:processID"));
  ok(caps.has("moz:profile"));
  equal(false, caps.get("moz:accessibilityChecks"));

  run_next_test();
});

add_test(function test_Capabilities_toString() {
  equal("[object session.Capabilities]", new session.Capabilities().toString());

  run_next_test();
});

add_test(function test_Capabilities_toJSON() {
  let caps = new session.Capabilities();
  let json = caps.toJSON();

  equal(caps.get("browserName"), json.browserName);
  equal(caps.get("browserVersion"), json.browserVersion);
  equal(caps.get("platformName"), json.platformName);
  equal(caps.get("platformVersion"), json.platformVersion);
  equal(caps.get("pageLoadStrategy"), json.pageLoadStrategy);
  equal(caps.get("acceptInsecureCerts"), json.acceptInsecureCerts);
  deepEqual(caps.get("timeouts").toJSON(), json.timeouts);
  equal(undefined, json.proxy);

  equal(caps.get("rotatable"), json.rotatable);

  equal(caps.get("specificationLevel"), json.specificationLevel);
  equal(caps.get("moz:processID"), json["moz:processID"]);
  equal(caps.get("moz:profile"), json["moz:profile"]);
  equal(caps.get("moz:accessibilityChecks"), json["moz:accessibilityChecks"]);

  run_next_test();
});

add_test(function test_Capabilities_fromJSON() {
  const {fromJSON} = session.Capabilities;

  // plain
  for (let typ of [{}, null, undefined]) {
    ok(fromJSON(typ, {merge: true}).has("browserName"));
    ok(fromJSON(typ, {merge: false}).has("browserName"));
  }
  for (let typ of [true, 42, "foo", []]) {
    Assert.throws(() =>
        fromJSON(typ, {merge: true}), InvalidArgumentError);
    Assert.throws(() =>
        fromJSON(typ, {merge: false}), InvalidArgumentError);
  }

  // merging
  let desired = {"moz:accessibilityChecks": false};
  let required = {"moz:accessibilityChecks": true};
  let matched = fromJSON(
      {desiredCapabilities: desired, requiredCapabilities: required},
      {merge: true});
  ok(matched.has("moz:accessibilityChecks"));
  equal(true, matched.get("moz:accessibilityChecks"));

  // desiredCapabilities/requriedCapabilities types
  for (let typ of [undefined, null, {}]) {
    ok(fromJSON({desiredCapabilities: typ}, {merge: true}));
    ok(fromJSON({requiredCapabilities: typ}, {merge: true}));
  }
  for (let typ of [true, 42, "foo", []]) {
    Assert.throws(() => fromJSON({desiredCapabilities: typ}, {merge: true}));
    Assert.throws(() => fromJSON({requiredCapabilities: typ}, {merge: true}));
  }

  // matching
  let caps = new session.Capabilities();

  ok(fromJSON({browserName: caps.get("browserName")}));
  ok(fromJSON({browserName: null}));
  ok(fromJSON({browserName: undefined}));
  ok(fromJSON({browserName: "*"}));
  Assert.throws(() => fromJSON({browserName: "foo"}));

  ok(fromJSON({browserVersion: caps.get("browserVersion")}));
  ok(fromJSON({browserVersion: null}));
  ok(fromJSON({browserVersion: undefined}));
  ok(fromJSON({browserVersion: "*"}));
  Assert.throws(() => fromJSON({browserVersion: "foo"}));

  ok(fromJSON({platformName: caps.get("platformName")}));
  ok(fromJSON({platformName: null}));
  ok(fromJSON({platformName: undefined}));
  ok(fromJSON({platformName: "*"}));
  Assert.throws(() => fromJSON({platformName: "foo"}));

  ok(fromJSON({platformVersion: caps.get("platformVersion")}));
  ok(fromJSON({platformVersion: null}));
  ok(fromJSON({platformVersion: undefined}));
  ok(fromJSON({platformVersion: "*"}));
  Assert.throws(() => fromJSON({platformVersion: "foo"}));

  caps = fromJSON({acceptInsecureCerts: true});
  equal(true, caps.get("acceptInsecureCerts"));
  caps = fromJSON({acceptInsecureCerts: false});
  equal(false, caps.get("acceptInsecureCerts"));
  Assert.throws(() => fromJSON({acceptInsecureCerts: "foo"}));

  for (let strategy of Object.values(session.PageLoadStrategy)) {
    caps = fromJSON({pageLoadStrategy: strategy});
    equal(strategy, caps.get("pageLoadStrategy"));
  }
  Assert.throws(() => fromJSON({pageLoadStrategy: "foo"}));

  let proxyConfig = {proxyType: "manual"};
  caps = fromJSON({proxy: proxyConfig});
  equal("manual", caps.get("proxy").proxyType);

  let timeoutsConfig = {implicit: 123};
  caps = fromJSON({timeouts: timeoutsConfig});
  equal(123, caps.get("timeouts").implicit);

  equal(0, caps.get("specificationLevel"));
  caps = fromJSON({specificationLevel: 123});
  equal(123, caps.get("specificationLevel"));
  Assert.throws(() => fromJSON({specificationLevel: "foo"}));
  Assert.throws(() => fromJSON({specificationLevel: -1}));

  caps = fromJSON({"moz:accessibilityChecks": true});
  equal(true, caps.get("moz:accessibilityChecks"));
  caps = fromJSON({"moz:accessibilityChecks": false});
  equal(false, caps.get("moz:accessibilityChecks"));
  Assert.throws(() => fromJSON({"moz:accessibilityChecks": "foo"}));

  run_next_test();
});

// use session.Proxy.toJSON to test marshal
add_test(function test_marshal() {
  let proxy = new session.Proxy();

  // drop empty fields
  deepEqual({}, proxy.toJSON());
  proxy.proxyType = "manual";
  deepEqual({proxyType: "manual"}, proxy.toJSON());
  proxy.proxyType = null;
  deepEqual({}, proxy.toJSON());
  proxy.proxyType = undefined;
  deepEqual({}, proxy.toJSON());

  // iterate over object literals
  proxy.proxyType = {foo: "bar"};
  deepEqual({proxyType: {foo: "bar"}}, proxy.toJSON());

  // iterate over complex object that implement toJSON
  proxy.proxyType = new session.Proxy();
  deepEqual({}, proxy.toJSON());
  proxy.proxyType.proxyType = "manual";
  deepEqual({proxyType: {proxyType: "manual"}}, proxy.toJSON());

  // drop objects with no entries
  proxy.proxyType = {foo: {}};
  deepEqual({}, proxy.toJSON());
  proxy.proxyType = {foo: new session.Proxy()};
  deepEqual({}, proxy.toJSON());

  run_next_test();
});
