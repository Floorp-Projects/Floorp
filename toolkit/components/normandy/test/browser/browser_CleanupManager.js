"use strict";

ChromeUtils.import(
  "resource://normandy/lib/CleanupManager.jsm",
  this
); /* global CleanupManagerClass */

add_task(async function testCleanupManager() {
  const spy1 = sinon.spy();
  const spy2 = sinon.spy();
  const spy3 = sinon.spy();

  const manager = new CleanupManager.constructor();
  manager.addCleanupHandler(spy1);
  manager.addCleanupHandler(spy2);
  manager.addCleanupHandler(spy3);
  manager.removeCleanupHandler(spy2); // Test removal

  await manager.cleanup();
  ok(spy1.called, "cleanup called the spy1 handler");
  ok(!spy2.called, "cleanup did not call the spy2 handler");
  ok(spy3.called, "cleanup called the spy3 handler");

  await manager.cleanup();
  ok(spy1.calledOnce, "cleanup only called the spy1 handler once");
  ok(spy3.calledOnce, "cleanup only called the spy3 handler once");
});
