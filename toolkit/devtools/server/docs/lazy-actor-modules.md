Lazy Actor Modules and E10S setup
---------------------------------

The **DebuggerServer** loads and creates most of the actors lazily to keep
the initial memory usage down (which is extremely important on lower end devices).

## Register a lazy global/tab actor module

register a global actor:

```js
    DebuggerServer.registerModule("devtools/server/actors/webapps", {
      prefix: "webapps",
      constructor: "WebappsActor",
      type: { global: true }
    });
```

register a tab actor:

```js
    DebuggerServer.registerModule("devtools/server/actors/webconsole", {
      prefix: "console",
      constructor: "WebConsoleActor",
      type: { tab: true }
    });
```

## E10S Setup

Some of the actor modules needs to exchange messages between the parent and child processes.

E.g. the **director-manager** needs to ask the list installed **director scripts** from
the **director-registry** running in the parent process) and the parent/child setup
is lazily directed by the **DebuggerServer**.

When the actor is loaded for the first time in the the **DebuggerServer** running in the
child process, it has the chances to run its setup procedure, e.g. in the **director-registry**:

```js
...
const {DebuggerServer} = require("devtools/server/main");

...

// skip child setup if this actor module is not running in a child process
if (DebuggerServer.isInChildProcess) {
  setupChildProcess();
}
...
```

The above setupChildProcess helper will use the **DebuggerServer.setupInParent**
to start a setup process in the parent process Debugger Server, e.g. in the the **director-registry**:

```js
function setupChildProcess() {
  const { sendSyncMessage } = DebuggerServer.parentMessageManager;

  DebuggerServer.setupInParent({
    module: "devtools/server/actors/director-registry",
    setupParent: "setupParentProcess"
  });

  ...
```

in the parent process, the **DebuggerServer** will require the requested module
and call the **setupParent** exported helper with the **MessageManager**
connected to the child process as parameter, e.g. in the **director-registry**:

```js
/**
 * E10S parent/child setup helpers
 */

let gTrackedMessageManager = new Set();

exports.setupParentProcess = function setupParentProcess({ mm, prefix }) {
  if (gTrackedMessageManager.has(mm)) { return; }
  gTrackedMessageManager.add(mm);

  // listen for director-script requests from the child process
  mm.addMessageListener("debug:director-registry-request", handleChildRequest);

  // time to unsubscribe from the disconnected message manager
  DebuggerServer.once("disconnected-from-child:" + prefix, handleMessageManagerDisconnected);

  function handleMessageManagerDisconnected(evt, { mm: disconnected_mm }) {
    ...
  }
```

The DebuggerServer emits "disconnected-from-child:CHILDID" events to give the actor modules
the chance to cleanup their handlers registered on the disconnected message manager.

## E10S setup flow

In the child process:
- DebuggerServer loads an actor module
  - the actor module check DebuggerServer.isInChildProcess 
    - the actor module calls the DebuggerServer.setupInParent helper
    - the DebuggerServer.setupInParent helper asks to the parent process
      to run the required setup
    - the actor module use the DebuggerServer.parentMessageManager.sendSyncMessage,
      DebuggerServer.parentMessageManager.addMessageListener helpers to send requests
      or to subscribe message handlers
      
In the parent process:
- The DebuggerServer receives the DebuggerServer.setupInParent request
- it tries to load the required module
- it tries to call the **mod[setupParent]** method with the frame message manager and the prefix
  in the json parameter **{ mm, prefix }**
  - the module setupParent helper use the mm to subscribe the messagemanager events
  - the module setupParent helper use the DebuggerServer object to subscribe *once* the
    **"disconnected-from-child:PREFIX"** event (needed to unsubscribe the messagemanager events)
