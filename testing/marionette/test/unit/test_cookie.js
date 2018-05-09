/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("chrome://marionette/content/cookie.js");

/* eslint-disable mozilla/use-chromeutils-generateqi */

cookie.manager = {
  cookies: [],

  add(domain, path, name, value, secure, httpOnly, session, expiry, originAttributes) {
    if (name === "fail") {
      throw new Error("An error occurred while adding cookie");
    }
    let newCookie = {
      host: domain,
      path,
      name,
      value,
      isSecure: secure,
      isHttpOnly: httpOnly,
      isSession: session,
      expiry,
      originAttributes,
    };
    cookie.manager.cookies.push(newCookie);
  },

  remove(host, name, path) {
    for (let i = 0; i < this.cookies.length; ++i) {
      let candidate = this.cookies[i];
      if (candidate.host === host &&
          candidate.name === name &&
          candidate.path === path) {
        return this.cookies.splice(i, 1);
      }
    }
    return false;
  },

  getCookiesFromHost(host) {
    let hostCookies = this.cookies.filter(cookie => cookie.host === host ||
       cookie.host === "." + host);
    let nextIndex = 0;

    return {
      hasMoreElements() {
        return nextIndex < hostCookies.length;
      },

      getNext() {
        return {
          QueryInterface() {
            return hostCookies[nextIndex++];
          },
        };
      },
    };
  },
};

add_test(function test_fromJSON() {
  // object
  for (let invalidType of ["foo", 42, true, [], null, undefined]) {
    Assert.throws(() => cookie.fromJSON(invalidType), /Expected cookie object/);
  }

  // name and value
  for (let invalidType of [42, true, [], {}, null, undefined]) {
    Assert.throws(() => cookie.fromJSON({name: invalidType}), /Cookie name must be string/);
    Assert.throws(() => cookie.fromJSON({name: "foo", value: invalidType}), /Cookie value must be string/);
  }

  // domain
  for (let invalidType of [42, true, [], {}, null]) {
    let domainTest = {
      name: "foo",
      value: "bar",
      domain: invalidType,
    };
    Assert.throws(() => cookie.fromJSON(domainTest), /Cookie domain must be string/);
  }
  let domainTest = {
    name: "foo",
    value: "bar",
    domain: "domain",
  };
  let parsedCookie = cookie.fromJSON(domainTest);
  equal(parsedCookie.domain, "domain");

  // path
  for (let invalidType of [42, true, [], {}, null]) {
    let pathTest = {
      name: "foo",
      value: "bar",
      path: invalidType,
    };
    Assert.throws(() => cookie.fromJSON(pathTest), /Cookie path must be string/);
  }

  // secure
  for (let invalidType of ["foo", 42, [], {}, null]) {
    let secureTest = {
      name: "foo",
      value: "bar",
      secure: invalidType,
    };
    Assert.throws(() => cookie.fromJSON(secureTest), /Cookie secure flag must be boolean/);
  }

  // httpOnly
  for (let invalidType of ["foo", 42, [], {}, null]) {
    let httpOnlyTest = {
      name: "foo",
      value: "bar",
      httpOnly: invalidType,
    };
    Assert.throws(() => cookie.fromJSON(httpOnlyTest), /Cookie httpOnly flag must be boolean/);
  }

  // expiry
  for (let invalidType of [-1, Number.MAX_SAFE_INTEGER + 1, "foo", true, [], {}, null]) {
    let expiryTest = {
      name: "foo",
      value: "bar",
      expiry: invalidType,
    };
    Assert.throws(() => cookie.fromJSON(expiryTest), /Cookie expiry must be a positive integer/);
  }

  // bare requirements
  let bare = cookie.fromJSON({name: "name", value: "value"});
  equal("name", bare.name);
  equal("value", bare.value);
  for (let missing of ["path", "secure", "httpOnly", "session", "expiry"]) {
    ok(!bare.hasOwnProperty(missing));
  }

  // everything
  let full = cookie.fromJSON({
    name: "name",
    value: "value",
    domain: ".domain",
    path: "path",
    secure: true,
    httpOnly: true,
    expiry: 42,
  });
  equal("name", full.name);
  equal("value", full.value);
  equal(".domain", full.domain);
  equal("path", full.path);
  equal(true, full.secure);
  equal(true, full.httpOnly);
  equal(42, full.expiry);

  run_next_test();
});

add_test(function test_add() {
  cookie.manager.cookies = [];

  for (let invalidType of [42, true, [], {}, null, undefined]) {
    Assert.throws(
        () => cookie.add({name: invalidType}),
        /Cookie name must be string/);
    Assert.throws(
        () => cookie.add({name: "name", value: invalidType}),
        /Cookie value must be string/);
    Assert.throws(
        () => cookie.add({name: "name", value: "value", domain: invalidType}),
        /Cookie domain must be string/);
  }

  cookie.add({
    name: "name",
    value: "value",
    domain: "domain",
  });
  equal(1, cookie.manager.cookies.length);
  equal("name", cookie.manager.cookies[0].name);
  equal("value", cookie.manager.cookies[0].value);
  equal(".domain", cookie.manager.cookies[0].host);
  equal("/", cookie.manager.cookies[0].path);
  ok(cookie.manager.cookies[0].expiry > new Date(Date.now()).getTime() / 1000);

  cookie.add({
    name: "name2",
    value: "value2",
    domain: "domain2",
  });
  equal(2, cookie.manager.cookies.length);

  Assert.throws(() => {
    let biscuit = {name: "name3", value: "value3", domain: "domain3"};
    cookie.add(biscuit, {restrictToHost: "other domain"});
  }, /Cookies may only be set for the current domain/);

  cookie.add({
    name: "name4",
    value: "value4",
    domain: "my.domain:1234",
  });
  equal(".my.domain", cookie.manager.cookies[2].host);

  cookie.add({
    name: "name5",
    value: "value5",
    domain: "domain5",
    path: "/foo/bar",
  });
  equal("/foo/bar", cookie.manager.cookies[3].path);

  cookie.add({
    name: "name6",
    value: "value",
    domain: ".domain",
  });
  equal(".domain", cookie.manager.cookies[4].host);

  Assert.throws(() => {
    cookie.add({name: "fail", value: "value6", domain: "domain6"});
  }, /UnableToSetCookieError/);

  run_next_test();
});

add_test(function test_remove() {
  cookie.manager.cookies = [];

  let crumble = {
    name: "test_remove",
    value: "value",
    domain: "domain",
    path: "/custom/path",
  };

  equal(0, cookie.manager.cookies.length);
  cookie.add(crumble);
  equal(1, cookie.manager.cookies.length);

  cookie.remove(crumble);
  equal(0, cookie.manager.cookies.length);
  equal(undefined, cookie.manager.cookies[0]);

  run_next_test();
});

add_test(function test_iter() {
  cookie.manager.cookies = [];
  let tomorrow = new Date();
  tomorrow.setHours(tomorrow.getHours() + 24);

  cookie.add({
    expiry: tomorrow,
    name: "0",
    value: "",
    domain: "foo.example.com",
  });
  cookie.add({
    expiry: tomorrow,
    name: "1",
    value: "",
    domain: "bar.example.com",
  });

  let fooCookies = [...cookie.iter("foo.example.com")];
  equal(1, fooCookies.length);
  equal(".foo.example.com", fooCookies[0].domain);
  equal(true, fooCookies[0].hasOwnProperty("expiry"));

  cookie.add({
    name: "aSessionCookie",
    value: "",
    domain: "session.com",
  });

  let sessionCookies = [...cookie.iter("session.com")];
  equal(1, sessionCookies.length);
  equal("aSessionCookie", sessionCookies[0].name);
  equal(false, sessionCookies[0].hasOwnProperty("expiry"));

  run_next_test();
});
