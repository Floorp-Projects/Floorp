"use strict";

const global = this;

Cu.import("resource://gre/modules/Timer.jsm");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  BaseContext,
  EventManager,
  SingletonEventManager,
} = ExtensionUtils;

class StubContext extends BaseContext {
  constructor() {
    super();
    this.sandbox = new Cu.Sandbox(global);
  }

  get cloneScope() {
    return this. sandbox;
  }

  get extension() {
    return {id: "test@web.extension"};
  }
}


add_task(function* test_post_unload_promises() {
  let context = new StubContext();

  let fail = result => {
    ok(false, `Unexpected callback: ${result}`);
  };

  // Make sure promises resolve normally prior to unload.
  let promises = [
    context.wrapPromise(Promise.resolve()),
    context.wrapPromise(Promise.reject({message: ""})).catch(() => {}),
  ];

  yield Promise.all(promises);

  // Make sure promises that resolve after unload do not trigger
  // resolution handlers.

  context.wrapPromise(Promise.resolve("resolved"))
         .then(fail);

  context.wrapPromise(Promise.reject({message: "rejected"}))
         .then(fail, fail);

  context.unload();

  // The `setTimeout` ensures that we return to the event loop after
  // promise resolution, which means we're guaranteed to return after
  // any micro-tasks that get enqueued by the resolution handlers above.
  yield new Promise(resolve => setTimeout(resolve, 0));
});


add_task(function* test_post_unload_listeners() {
  let context = new StubContext();

  let fireEvent;
  let onEvent = new EventManager(context, "onEvent", fire => {
    fireEvent = fire;
    return () => {};
  });

  let fireSingleton;
  let onSingleton = new SingletonEventManager(context, "onSingleton", callback => {
    fireSingleton = () => {
      Promise.resolve().then(callback);
    };
    return () => {};
  });

  let fail = event => {
    ok(false, `Unexpected event: ${event}`);
  };

  // Check that event listeners aren't called after they've been removed.
  onEvent.addListener(fail);
  onSingleton.addListener(fail);

  let promises = [
    new Promise(resolve => onEvent.addListener(resolve)),
    new Promise(resolve => onSingleton.addListener(resolve)),
  ];

  fireEvent("onEvent");
  fireSingleton("onSingleton");

  // Both `fireEvent` calls are dispatched asynchronously, so they won't
  // have fired by this point. The `fail` listeners that we remove now
  // should not be called, even though the events have already been
  // enqueued.
  onEvent.removeListener(fail);
  onSingleton.removeListener(fail);

  // Wait for the remaining listeners to be called, which should always
  // happen after the `fail` listeners would normally be called.
  yield Promise.all(promises);

  // Check that event listeners aren't called after the context has
  // unloaded.
  onEvent.addListener(fail);
  onSingleton.addListener(fail);

  // The EventManager `fire` callback always dispatches events
  // asynchronously, so we need to test that any pending event callbacks
  // aren't fired after the context unloads. We also need to test that
  // any `fire` calls that happen *after* the context is unloaded also
  // do not trigger callbacks.
  fireEvent("onEvent");
  Promise.resolve("onEvent").then(fireEvent);

  fireSingleton("onSingleton");
  Promise.resolve("onSingleton").then(fireSingleton);

  context.unload();

  // The `setTimeout` ensures that we return to the event loop after
  // promise resolution, which means we're guaranteed to return after
  // any micro-tasks that get enqueued by the resolution handlers above.
  yield new Promise(resolve => setTimeout(resolve, 0));
});
