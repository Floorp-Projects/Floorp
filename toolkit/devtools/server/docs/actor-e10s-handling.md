# How to handle E10S in actors

In multi-process environments, most devtools actors are created and initialized in the child content process, to be able to access the resources they are exposing to the toolbox. But sometimes, these actors need to access things in the parent process too. Here's why and how.

## Use case and examples

Some actors need to exchange messages between the parent and the child process (typically when some components aren't available in the child process).

E.g. the **director-manager** needs to ask the list of installed **director scripts** from
the **director-registry** running in the parent process.

To that end, there's a parent/child setup mechanism at `DebuggerServer` level that can be used.

When the actor is loaded for the first time in the `DebuggerServer` running in the child process, it may decide to run a setup procedure to load a module in the parent process with which to communicate.

E.g. in the **director-registry**:

```
  const {DebuggerServer} = require("devtools/server/main");

  // Setup the child<->parent communication only if the actor module
  // is running in a child process.
  if (DebuggerServer.isInChildProcess) {
    setupChildProcess();
  }

  function setupChildProcess() {
    DebuggerServer.setupInParent({
      module: "devtools/server/actors/director-registry",
      setupParent: "setupParentProcess"
    });
    // ...
  }
```

The `setupChildProcess` helper defined and used in the previous example uses the `DebuggerServer.setupInParent` to run a given setup function in the parent process Debugger Server, e.g. in the **director-registry** module.

With this, the `DebuggerServer` running in the parent process will require the requested module (**director-registry**) and call its `setupParentProcess` function (which should be exported on the module).

The `setupParentProcess` function will receive a parameter that contains a reference to the **MessageManager** and a prefix that should be used to send/receive messages between the child and parent processes.

See below an example implementation of a `setupParent` function in the parent process:

```
let gTrackedMessageManager = new Set();
exports.setupParentProcess = function setupParentProcess({ mm, prefix }) {
  // Prevent multiple subscriptions on the same messagemanager.
  if (gTrackedMessageManager.has(mm)) { return; }
  gTrackedMessageManager.add(mm);

  // Start listening for messages from the actor in the child process.
  mm.addMessageListener("debug:some-message-name", handleChildRequest);

  function handleChildRequest(msg) {
    switch (msg.json.method) {
      case "get":
        return doGetInParentProcess(msg.json.args[0]);
        break;
      case "list":
        return doListInParentProcess();
        break;
      default:
        console.error("Unknown method name", msg.json.method);
        throw new Error("Unknown method name");
    }
  }

  // Listen to the disconnection message to clean-up.
  DebuggerServer.once("disconnected-from-child:" + prefix, handleMessageManagerDisconnected);

  function handleMessageManagerDisconnected(evt, { mm: disconnected_mm }) {
    // filter out not subscribed message managers
    if (disconnected_mm !== mm || !gTrackedMessageManager.has(mm)) {
      return;
    }

    gTrackedMessageManager.delete(mm);

    // unregister for director-script requests handlers from the parent process (if any)
    mm.removeMessageListener("debug:director-registry-request", handleChildRequest);
  }
```

The `DebuggerServer` emits "disconnected-from-child:CHILDID" events to give the actor modules the chance to cleanup their handlers registered on the disconnected message manager.

## Summary of the setup flow

In the child process:

* The `DebuggerServer` loads an actor module,
* the actor module checks `DebuggerServer.isInChildProcess` to know whether it runs in a child process or not,
* the actor module then uses the `DebuggerServer.setupInParent` helper to start setting up a parent-process counterpart,
* the `DebuggerServer.setupInParent` helper asks the parent process to run the required module's setup function,
* the actor module uses the `DebuggerServer.parentMessageManager.sendSyncMessage` and `DebuggerServer.parentMessageManager.addMessageListener` helpers to send or listen to message.

In the parent process:

* The DebuggerServer receives the `DebuggerServer.setupInParent` request,
* tries to load the required module,
* tries to call the `module[setupParent]` function with the frame message manager and the prefix as parameters `{ mm, prefix }`,
* the `setupParent` function then uses the mm to subscribe the messagemanager events,
* the `setupParent` function also uses the DebuggerServer object to subscribe *once* to the `"disconnected-from-child:PREFIX"` event to unsubscribe from messagemanager events.