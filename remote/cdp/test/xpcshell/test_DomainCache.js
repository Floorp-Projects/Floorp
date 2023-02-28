/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Domain } = ChromeUtils.importESModule(
  "chrome://remote/content/cdp/domains/Domain.sys.mjs"
);
const { DomainCache } = ChromeUtils.importESModule(
  "chrome://remote/content/cdp/domains/DomainCache.sys.mjs"
);

class MockSession {
  onEvent() {}
}

const noopSession = new MockSession();

add_task(function test_DomainCache_constructor() {
  new DomainCache(noopSession, {});
});

add_task(function test_DomainCache_domainSupportsMethod() {
  const modules = {
    Foo: class extends Domain {
      bar() {}
    },
  };
  const domains = new DomainCache(noopSession, modules);

  ok(domains.domainSupportsMethod("Foo", "bar"));
  ok(!domains.domainSupportsMethod("Foo", "baz"));
  ok(!domains.domainSupportsMethod("foo", "bar"));
});

add_task(function test_DomainCache_get_invalidModule() {
  Assert.throws(() => {
    const domains = new DomainCache(noopSession, { Foo: undefined });
    domains.get("Foo");
  }, /UnknownMethodError/);
});

add_task(function test_DomainCache_get_missingConstructor() {
  Assert.throws(() => {
    const domains = new DomainCache(noopSession, { Foo: {} });
    domains.get("Foo");
  }, /TypeError/);
});

add_task(function test_DomainCache_get_superClassNotDomain() {
  Assert.throws(() => {
    const domains = new DomainCache(noopSession, { Foo: class {} });
    domains.get("Foo");
  }, /TypeError/);
});

add_task(function test_DomainCache_get_constructs() {
  let eventFired;
  class Session {
    onEvent(event) {
      eventFired = event;
    }
  }

  let constructed = false;
  class Foo extends Domain {
    constructor() {
      super();
      constructed = true;
    }
  }

  const session = new Session();
  const domains = new DomainCache(session, { Foo });

  const foo = domains.get("Foo");
  ok(constructed);
  ok(foo instanceof Foo);

  const event = {};
  foo.emit(event);
  equal(event, eventFired);
});

add_task(function test_DomainCache_size() {
  class Foo extends Domain {}
  const domains = new DomainCache(noopSession, { Foo });

  equal(domains.size, 0);
  domains.get("Foo");
  equal(domains.size, 1);
});

add_task(function test_DomainCache_clear() {
  let dtorCalled = false;
  class Foo extends Domain {
    destructor() {
      dtorCalled = true;
    }
  }

  const domains = new DomainCache(noopSession, { Foo });

  equal(domains.size, 0);
  domains.get("Foo");
  equal(domains.size, 1);

  domains.clear();
  equal(domains.size, 0);
  ok(dtorCalled);
});
