Components.utils.import("resource://services-sync/ext/Observers.js");

let gSubject = {};

function test_function_observer() {
  let foo = false;

  let onFoo = function(subject, data) {
    foo = !foo;
    do_check_eq(subject, gSubject);
    do_check_eq(data, "some data");
  };

  Observers.add("foo", onFoo);
  Observers.notify("foo", gSubject, "some data");

  // The observer was notified after being added.
  do_check_true(foo);

  Observers.remove("foo", onFoo);
  Observers.notify("foo");

  // The observer was not notified after being removed.
  do_check_true(foo);
}

function test_method_observer() {
  let obj = {
    foo: false,
    onFoo: function(subject, data) {
      this.foo = !this.foo;
      do_check_eq(subject, gSubject);
      do_check_eq(data, "some data");
    }
  };

  // The observer is notified after being added.
  Observers.add("foo", obj.onFoo, obj);
  Observers.notify("foo", gSubject, "some data");
  do_check_true(obj.foo);

  // The observer is not notified after being removed.
  Observers.remove("foo", obj.onFoo, obj);
  Observers.notify("foo");
  do_check_true(obj.foo);
}

function test_object_observer() {
  let obj = {
    foo: false,
    observe: function(subject, topic, data) {
      this.foo = !this.foo;

      do_check_eq(subject, gSubject);
      do_check_eq(topic, "foo");
      do_check_eq(data, "some data");
    }
  };

  Observers.add("foo", obj);
  Observers.notify("foo", gSubject, "some data");

  // The observer is notified after being added.
  do_check_true(obj.foo);

  Observers.remove("foo", obj);
  Observers.notify("foo");

  // The observer is not notified after being removed.
  do_check_true(obj.foo);
}

function run_test() {
  test_function_observer();
  test_method_observer();
  test_object_observer();
}
