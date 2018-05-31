"use strict";

const global = this;

ChromeUtils.import("resource://gre/modules/Timer.jsm");

ChromeUtils.import("resource://gre/modules/ExtensionCommon.jsm");

var {
  BaseContext,
  EventManager,
} = ExtensionCommon;

class StubContext extends BaseContext {
  constructor() {
    let fakeExtension = {id: "test@web.extension"};
    super("testEnv", fakeExtension);
    this.sandbox = Cu.Sandbox(global);
  }

  get cloneScope() {
    return this.sandbox;
  }

  get principal() {
    return Cu.getObjectPrincipal(this.sandbox);
  }
}


add_task(async function test_post_unload_promises() {
  let context = new StubContext();

  let fail = result => {
    ok(false, `Unexpected callback: ${result}`);
  };

  // Make sure promises resolve normally prior to unload.
  let promises = [
    context.wrapPromise(Promise.resolve()),
    context.wrapPromise(Promise.reject({message: ""})).catch(() => {}),
  ];

  await Promise.all(promises);

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
  await new Promise(resolve => setTimeout(resolve, 0));
});


add_task(async function test_post_unload_listeners() {
  let context = new StubContext();

  let fire;
  let manager = new EventManager({
    context,
    name: "EventManager",
    register: _fire => {
      fire = () => {
        _fire.async();
      };
      return () => {};
    },
  });

  let fail = event => {
    ok(false, `Unexpected event: ${event}`);
  };

  // Check that event listeners isn't called after it has been removed.
  manager.addListener(fail);

  let promise = new Promise(resolve => manager.addListener(resolve));

  fire();

  // The `fireSingleton` call ia dispatched asynchronously, so it won't
  // have fired by this point. The `fail` listener that we remove now
  // should not be called, even though the event has already been
  // enqueued.
  manager.removeListener(fail);

  // Wait for the remaining listener to be called, which should always
  // happen after the `fail` listener would normally be called.
  await promise;

  // Check that the event listener isn't called after the context has
  // unloaded.
  manager.addListener(fail);

  // The `fire` callback always dispatches events
  // asynchronously, so we need to test that any pending event callbacks
  // aren't fired after the context unloads. We also need to test that
  // any `fire` calls that happen *after* the context is unloaded also
  // do not trigger callbacks.
  fire();
  Promise.resolve().then(fire);

  context.unload();

  // The `setTimeout` ensures that we return to the event loop after
  // promise resolution, which means we're guaranteed to return after
  // any micro-tasks that get enqueued by the resolution handlers above.
  await new Promise(resolve => setTimeout(resolve, 0));
});

class Context extends BaseContext {
  constructor(principal) {
    let fakeExtension = {id: "test@web.extension"};
    super("testEnv", fakeExtension);
    Object.defineProperty(this, "principal", {
      value: principal,
      configurable: true,
    });
    this.sandbox = Cu.Sandbox(principal, {wantXrays: false});
  }

  get cloneScope() {
    return this.sandbox;
  }
}

let ssm = Services.scriptSecurityManager;
const PRINCIPAL1 = ssm.createCodebasePrincipalFromOrigin("http://www.example.org");
const PRINCIPAL2 = ssm.createCodebasePrincipalFromOrigin("http://www.somethingelse.org");

// Test that toJSON() works in the json sandbox
add_task(async function test_stringify_toJSON() {
  let context = new Context(PRINCIPAL1);
  let obj = Cu.evalInSandbox("({hidden: true, toJSON() { return {visible: true}; } })", context.sandbox);

  let stringified = context.jsonStringify(obj);
  let expected = JSON.stringify({visible: true});
  equal(stringified, expected, "Stringified object with toJSON() method is as expected");
});

// Test that stringifying in inaccessible property throws
add_task(async function test_stringify_inaccessible() {
  let context = new Context(PRINCIPAL1);
  let sandbox = context.sandbox;
  let sandbox2 = Cu.Sandbox(PRINCIPAL2);

  Cu.waiveXrays(sandbox).subobj = Cu.evalInSandbox("({ subobject: true })", sandbox2);
  let obj = Cu.evalInSandbox("({ local: true, nested: subobj })", sandbox);
  Assert.throws(() => {
    context.jsonStringify(obj);
  }, /Permission denied to access property "toJSON"/);
});

add_task(async function test_stringify_accessible() {
  // Test that an accessible property from another global is included
  let principal = Cu.getObjectPrincipal(Cu.Sandbox([PRINCIPAL1, PRINCIPAL2]));
  let context = new Context(principal);
  let sandbox = context.sandbox;
  let sandbox2 = Cu.Sandbox(PRINCIPAL2);

  Cu.waiveXrays(sandbox).subobj = Cu.evalInSandbox("({ subobject: true })", sandbox2);
  let obj = Cu.evalInSandbox("({ local: true, nested: subobj })", sandbox);
  let stringified = context.jsonStringify(obj);

  let expected = JSON.stringify({local: true, nested: {subobject: true}});
  equal(stringified, expected, "Stringified object with accessible property is as expected");
});
