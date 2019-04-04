/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {Domain} = ChromeUtils.import("chrome://remote/content/domains/Domain.jsm");
const {Domains} = ChromeUtils.import("chrome://remote/content/domains/Domains.jsm");

class MockSession {
  onEvent() {}
}

const noopSession = new MockSession();

add_test(function test_Domains_constructor() {
  new Domains(noopSession, {});

  run_next_test();
});

add_test(function test_Domains_domainSupportsMethod() {
  const modules = {
    "Foo": class extends Domain {
      bar() {}
    },
  };
  const domains = new Domains(noopSession, modules);

  ok(domains.domainSupportsMethod("Foo", "bar"));
  ok(!domains.domainSupportsMethod("Foo", "baz"));
  ok(!domains.domainSupportsMethod("foo", "bar"));

  run_next_test();
});

add_test(function test_Domains_get_invalidModule() {
  Assert.throws(() => {
    const domains = new Domains(noopSession, {Foo: undefined});
    domains.get("Foo");
  }, /UnknownMethodError/);

  run_next_test();
});

add_test(function test_Domains_get_missingConstructor() {
  Assert.throws(() => {
    const domains = new Domains(noopSession, {Foo: {}});
    domains.get("Foo");
  }, /TypeError/);

  run_next_test();
});

add_test(function test_Domain_get_superClassNotDomain() {
  Assert.throws(() => {
    const domains = new Domain(noopSession, {Foo: class {}});
    domains.get("Foo");
  }, /TypeError/);

  run_next_test();
});

add_test(function test_Domains_get_constructs() {
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
  const domains = new Domains(session, {Foo});

  const foo = domains.get("Foo");
  ok(constructed);
  ok(foo instanceof Foo);

  const event = {};
  foo.emit(event);
  equal(event, eventFired);

  run_next_test();
});

add_test(function test_Domains_size() {
  class Foo extends Domain {}
  const domains = new Domains(noopSession, {Foo});

  equal(domains.size, 0);
  domains.get("Foo");
  equal(domains.size, 1);

  run_next_test();
});

add_test(function test_Domains_clear() {
  let dtorCalled = false;
  class Foo extends Domain {
    destructor() {
      dtorCalled = true;
    }
  }

  const domains = new Domains(noopSession, {Foo});

  equal(domains.size, 0);
  domains.get("Foo");
  equal(domains.size, 1);

  domains.clear();
  equal(domains.size, 0);
  ok(dtorCalled);

  run_next_test();
});

add_test(function test_Domains_splitMethod() {
  for (const t of [42, null, true, {}, [], undefined]) {
    Assert.throws(() => Domains.splitMethod(t), /TypeError/, `${typeof t} throws`);
  }
  for (const s of ["", ".", "foo.", ".bar", "foo.bar.baz"]) {
    Assert.throws(() => Domains.splitMethod(s), /Invalid method format: ".*"/, `"${s}" throws`);
  }
  deepEqual(Domains.splitMethod("foo.bar"), {domain: "foo", command: "bar"});

  run_next_test();
});
