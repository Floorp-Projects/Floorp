"use strict";

var EXPORTED_SYMBOLS = ["TestWorkerWatcherChild"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "wdm",
  "@mozilla.org/dom/workers/workerdebuggermanager;1",
  "nsIWorkerDebuggerManager"
);

class TestWorkerWatcherChild extends JSProcessActorChild {
  async receiveMessage(msg) {
    switch (msg.name) {
      case "Test:StartWatchingWorkers":
        this.startWatchingWorkers();
        break;
      case "Test:StopWatchingWorkers":
        this.stopWatchingWorkers();
        break;
      default:
        // Ensure the test case will fail if this JSProcessActorChild does receive
        // unexpected messages.
        return Promise.reject(
          new Error(`Unexpected message received: ${msg.name}`)
        );
    }
  }

  startWatchingWorkers() {
    if (!this._workerDebuggerListener) {
      const actor = this;
      this._workerDebuggerListener = {
        onRegister(dbg) {
          actor.sendAsyncMessage("Test:WorkerSpawned", {
            workerType: dbg.type,
            workerUrl: dbg.url,
          });
        },
        onUnregister(dbg) {
          actor.sendAsyncMessage("Test:WorkerTerminated", {
            workerType: dbg.type,
            workerUrl: dbg.url,
          });
        },
      };

      lazy.wdm.addListener(this._workerDebuggerListener);
    }
  }

  stopWatchingWorkers() {
    if (this._workerDebuggerListener) {
      lazy.wdm.removeListener(this._workerDebuggerListener);
      this._workerDebuggerListener = null;
    }
  }

  willDestroy() {
    this.stopWatchingWorkers();
  }
}
