/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://services-common/observers.js");

var gSubject = {};

add_test(function test_function_observer() {
  let foo = false;

  let onFoo = function(subject, data) {
    foo = !foo;
    Assert.equal(subject, gSubject);
    Assert.equal(data, "some data");
  };

  Observers.add("foo", onFoo);
  Observers.notify("foo", gSubject, "some data");

  // The observer was notified after being added.
  Assert.ok(foo);

  Observers.remove("foo", onFoo);
  Observers.notify("foo");

  // The observer was not notified after being removed.
  Assert.ok(foo);

  run_next_test();
});

add_test(function test_method_observer() {
  let obj = {
    foo: false,
    onFoo(subject, data) {
      this.foo = !this.foo;
      Assert.equal(subject, gSubject);
      Assert.equal(data, "some data");
    }
  };

  // The observer is notified after being added.
  Observers.add("foo", obj.onFoo, obj);
  Observers.notify("foo", gSubject, "some data");
  Assert.ok(obj.foo);

  // The observer is not notified after being removed.
  Observers.remove("foo", obj.onFoo, obj);
  Observers.notify("foo");
  Assert.ok(obj.foo);

  run_next_test();
});

add_test(function test_object_observer() {
  let obj = {
    foo: false,
    observe(subject, topic, data) {
      this.foo = !this.foo;

      Assert.equal(subject, gSubject);
      Assert.equal(topic, "foo");
      Assert.equal(data, "some data");
    }
  };

  Observers.add("foo", obj);
  Observers.notify("foo", gSubject, "some data");

  // The observer is notified after being added.
  Assert.ok(obj.foo);

  Observers.remove("foo", obj);
  Observers.notify("foo");

  // The observer is not notified after being removed.
  Assert.ok(obj.foo);

  run_next_test();
});
