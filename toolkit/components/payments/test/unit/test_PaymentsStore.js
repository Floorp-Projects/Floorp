"use strict";

/* import-globals-from ../../res/PaymentsStore.js */
Services.scriptloader.loadSubScript("resource://payments/PaymentsStore.js", this);

add_task(async function test_defaultState() {
  do_check_true(!!PaymentsStore, "Check PaymentsStore import");
  let ps = new PaymentsStore({
    foo: "bar",
  });

  let state = ps.getState();
  do_check_true(!!state, "Check state is truthy");
  do_check_eq(state.foo, "bar", "Check .foo");

  Assert.throws(() => state.foo = "new", TypeError, "Assigning to existing prop. should throw");
  Assert.throws(() => state.other = "something", TypeError, "Adding a new prop. should throw");
  Assert.throws(() => delete state.foo, TypeError, "Deleting a prop. should throw");
});

add_task(async function test_setState() {
  let ps = new PaymentsStore({});

  ps.setState({
    one: "one",
  });
  let state = ps.getState();
  do_check_eq(Object.keys(state).length, 1, "Should only have 1 prop. set");
  do_check_eq(state.one, "one", "Check .one");

  ps.setState({
    two: 2,
  });
  state = ps.getState();
  do_check_eq(Object.keys(state).length, 2, "Should have 2 props. set");
  do_check_eq(state.one, "one", "Check .one");
  do_check_eq(state.two, 2, "Check .two");

  ps.setState({
    one: "a",
    two: "b",
  });
  state = ps.getState();
  do_check_eq(state.one, "a", "Check .one");
  do_check_eq(state.two, "b", "Check .two");

  do_print("check consecutive setState for the same prop");
  ps.setState({
    one: "c",
  });
  ps.setState({
    one: "d",
  });
  state = ps.getState();
  do_check_eq(Object.keys(state).length, 2, "Should have 2 props. set");
  do_check_eq(state.one, "d", "Check .one");
  do_check_eq(state.two, "b", "Check .two");
});

add_task(async function test_subscribe_unsubscribe() {
  let ps = new PaymentsStore({});
  let subscriber = {
    stateChangePromise: null,
    _stateChangeResolver: null,

    reset() {
      this.stateChangePromise = new Promise(resolve => {
        this._stateChangeResolver = resolve;
      });
    },

    stateChangeCallback(state) {
      this._stateChangeResolver(state);
      this.stateChangePromise = new Promise(resolve => {
        this._stateChangeResolver = resolve;
      });
    },
  };

  sinon.spy(subscriber, "stateChangeCallback");
  subscriber.reset();
  ps.subscribe(subscriber);
  do_print("subscribe the same listener twice to ensure it still doesn't call the callback");
  ps.subscribe(subscriber);
  do_check_true(subscriber.stateChangeCallback.notCalled,
                "Check not called synchronously when subscribing");

  let changePromise = subscriber.stateChangePromise;
  ps.setState({
    a: 1,
  });
  do_check_true(subscriber.stateChangeCallback.notCalled,
                "Check not called synchronously for changes");
  let state = await changePromise;
  do_check_eq(state, subscriber.stateChangeCallback.getCall(0).args[0],
              "Check resolved state is last state");
  do_check_eq(JSON.stringify(state), `{"a":1}`, "Check callback state");

  do_print("Testing consecutive setState");
  subscriber.reset();
  subscriber.stateChangeCallback.reset();
  changePromise = subscriber.stateChangePromise;
  ps.setState({
    a: 2,
  });
  ps.setState({
    a: 3,
  });
  do_check_true(subscriber.stateChangeCallback.notCalled,
                "Check not called synchronously for changes");
  state = await changePromise;
  do_check_eq(state, subscriber.stateChangeCallback.getCall(0).args[0],
              "Check resolved state is last state");
  do_check_eq(JSON.stringify(subscriber.stateChangeCallback.getCall(0).args[0]), `{"a":3}`,
              "Check callback state matches second setState");

  do_print("test unsubscribe");
  subscriber.stateChangeCallback = function unexpectedChange() {
    do_check_true(false, "stateChangeCallback shouldn't be called after unsubscribing");
  };
  ps.unsubscribe(subscriber);
  ps.setState({
    a: 4,
  });
  await Promise.resolve("giving a chance for the callback to be called");
});
