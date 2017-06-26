/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const {utils: Cu} = Components;

Cu.import("chrome://marionette/content/cookie.js");

cookie.manager = {
  cookies: [],

  add: function (domain, path, name, value, secure, httpOnly, session, expiry, originAttributes) {
    let newCookie = {
      host: domain,
      path: path,
      name: name,
      value: value,
      isSecure: secure,
      isHttpOnly: httpOnly,
      isSession: session,
      expiry: expiry,
      originAttributes: originAttributes,
    };
    cookie.manager.cookies.push(newCookie);
  },

  remove: function (host, name, path, blocked, originAttributes) {;
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

  getCookiesFromHost(host, originAttributes = {}) {
    let hostCookies = this.cookies.filter(cookie => cookie.host === host);
    let nextIndex = 0;

    return {
      hasMoreElements () {
        return nextIndex < hostCookies.length;
      },

      getNext () {
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
    Assert.throws(() => cookie.fromJSON({name: invalidType}), "Cookie name must be string");
    Assert.throws(() => cookie.fromJSON({value: invalidType}), "Cookie value must be string");
  }

  // path
  for (let invalidType of [42, true, [], {}, null]) {
    let test = {
      name: "foo",
      value: "bar",
      path: invalidType,
    };
    Assert.throws(() => cookie.fromJSON(test), "Cookie path must be string");
  }

  // secure
  for (let invalidType of ["foo", 42, [], {}, null]) {
    let test = {
      name: "foo",
      value: "bar",
      secure: invalidType,
    };
    Assert.throws(() => cookie.fromJSON(test), "Cookie secure flag must be boolean");
  }

  // httpOnly
  for (let invalidType of ["foo", 42, [], {}, null]) {
    let test = {
      name: "foo",
      value: "bar",
      httpOnly: invalidType,
    };
    Assert.throws(() => cookie.fromJSON(test), "Cookie httpOnly flag must be boolean");
  }

  // session
  for (let invalidType of ["foo", 42, [], {}, null]) {
    let test = {
      name: "foo",
      value: "bar",
      session: invalidType,
    };
    Assert.throws(() => cookie.fromJSON(test), "Cookie session flag must be boolean");
  }

  // expiry
  for (let invalidType of ["foo", true, [], {}, null]) {
    let test = {
      name: "foo",
      value: "bar",
      expiry: invalidType,
    };
    Assert.throws(() => cookie.fromJSON(test), "Cookie expiry must be a positive integer");
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
    path: "path",
    secure: true,
    httpOnly: true,
    session: true,
    expiry: 42,
  });
  equal("name", full.name);
  equal("value", full.value);
  equal("path", full.path);
  equal(true, full.secure);
  equal(true, full.httpOnly);
  equal(true, full.session);
  equal(42, full.expiry);

  run_next_test();
});

add_test(function test_add() {
  cookie.manager.cookies = [];

  for (let invalidType of [42, true, [], {}, null, undefined]) {
    Assert.throws(
        () => cookie.add(invalidType),
        "Cookie must have string name");
    Assert.throws(
        () => cookie.add({name: invalidType}),
        "Cookie must have string name");
    Assert.throws(
        () => cookie.add({name: "name", value: invalidType}),
        "Cookie must have string value");
    Assert.throws(
        () => cookie.add({name: "name", value: "value", domain: invalidType}),
        "Cookie must have string value");
  }

  cookie.add({
    name: "name",
    value: "value",
    domain: "domain",
  });
  equal(1, cookie.manager.cookies.length);
  equal("name", cookie.manager.cookies[0].name);
  equal("value", cookie.manager.cookies[0].value);
  equal("domain", cookie.manager.cookies[0].host);
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
  }, "Cookies may only be set for the current domain");

  cookie.add({
    name: "name4",
    value: "value4",
    domain: "my.domain:1234",
  });
  equal("my.domain", cookie.manager.cookies[2].host);

  cookie.add({
    name: "name5",
    value: "value5",
    domain: "domain5",
    path: "/foo/bar",
  });
  equal("/foo/bar", cookie.manager.cookies[3].path);

  run_next_test();
});

add_test(function test_remove() {
  cookie.manager.cookies = [];

  let crumble = {
    name: "test_remove",
    value: "value",
    domain: "domain",
    path: "/custom/path"
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

  cookie.add({name: "0", value: "", domain: "example.com"});
  cookie.add({name: "1", value: "", domain: "foo.example.com"});
  cookie.add({name: "2", value: "", domain: "bar.example.com"});

  let fooCookies = [...cookie.iter("foo.example.com")];
  equal(1, fooCookies.length);
  equal("foo.example.com", fooCookies[0].domain);

  run_next_test();
});
