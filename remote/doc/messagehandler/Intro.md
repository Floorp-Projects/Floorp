# Introduction

## Overview

When developing browser tools in Firefox, you need to reach objects or APIs only available in certain layers (eg. processes or threads). There are powerful APIs available to communicate across layers (JSWindowActors, JSProcessActors) but they don't usually match all the needs from browser tool developers. For instance support for sessions, for events, ...

### Modules

The MessageHandler framework proposes to organize your code in modules, with the restriction that a given module can only run in a specific layer. Thanks to this, the framework will instantiate the modules where needed, and will provide easy ways to communicate between modules across layers. The goal is to take away all the complexity of routing information so that developers can simply focus on implementing the logic for their modules.

### Commands and Events

The framework is also designed around commands and events. Each module developed for the MessageHandler framework should expose commands and/or events. Commands follow a request/response model, and are conceptually similar to function calls where the caller could live in a different process than the callee. Events are emitted at the initiative of the module, and can reach listeners located in other layers. The role of modules is to implement the logic to handle commands (eg "click on an element") or generate events. The role of the framework is to send commands to modules, or to bubble events from modules. Commands and events are both used to communicate internally between modules, as well as externally with the consumer of your tooling modules.

The "MessageHandler" name comes from this role of "handling" commands and events, aka "messages".

### Summary

As a summary, the MessageHandler framework proposes to write tooling code as modules, which will run in various processes or threads, and communicate across layers using commands and events.

## Basic Architecture

### MessageHandler Network

Modules created for the MessageHandler framework need to run in several processes, threads, ...

To support this the framework will dynamically create a network of [MessageHandler](https://searchfox.org/mozilla-central/source/remote/shared/messagehandler/MessageHandler.sys.mjs) instances in the various layers that need to be accessed by your modules. The MessageHandler class is obviously named after the framework, but the name is appropriate because its role is mainly to route commands and events.

On top of routing duties, the MessageHandler class is also responsible for instantiating and managing modules. Typically, processing a command has two possible outcomes. Either it's not intended for this particular layer, in which case the MessageHandler will analyze the command and send it towards the appropriate recipient. But if it is intended for this layer, then the MessageHandler will try to delegate the command to the appropriate module. This means instantiating the module if it wasn't done before. So each node of a MessageHandler network also contains module instances.

The root of this network is the [RootMessageHandler](https://searchfox.org/mozilla-central/source/remote/shared/messagehandler/RootMessageHandler.sys.mjs) and lives in the parent process. For consumers, this is also the single entry point exposing the commands and events of your modules. It can also own module instances, if you have modules which are supposed to live in the parent process (aka root layer).

At the moment we only support another type of MessageHandler, the [WindowGlobalMessageHandler](https://searchfox.org/mozilla-central/source/remote/shared/messagehandler/WindowGlobalMessageHandler.sys.mjs) which will be used for the windowglobal layer and lives in the content process.

### Simplified architecture example

Let's imagine a very simple example, with a couple of modules:

* a root module called "version" with a command returning the current version of the browser

* a windowglobal module called "location" with a command returning the location of the windowglobal

Suppose the browser has 2 tabs, running in different processes. If the consumer used the "version" module, and the "location" module but only for one of the two tabs, the network will look like:

```text
 parent process                     content process 1
┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┐      ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─
                                     ╔═══════════════════════╗      ┌─────────────┐ │
│ ╔═══════════════════════╗ │      │ ║     WindowGlobal      ╠──────┤  location   │
  ║  RootMessageHandler   ║◀ ─ ─ ─ ─▶║    MessageHandler     ║      │   module    │ │
│ ╚══════════╦════════════╝ │      │ ╚═══════════════════════╝      └─────────────┘
             │                      ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┘
│            │              │
      ┌──────┴──────┐               content process 2
│     │   version   │       │      ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─
      │   module    │                                                               │
│     └─────────────┘       │      │
                                                                                    │
│                           │      │
 ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─        ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┘
```

But if the consumer sends another command, to retrieve the location of the other tab, the network will then evolve to:

```text
 parent process                     content process 1
┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┐      ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─
                                     ╔═══════════════════════╗      ┌─────────────┐ │
│ ╔═══════════════════════╗ │      │ ║     WindowGlobal      ╠──────┤  location   │
  ║  RootMessageHandler   ║◀ ─ ┬ ─ ─▶║    MessageHandler     ║      │   module    │ │
│ ╚══════════╦════════════╝ │      │ ╚═══════════════════════╝      └─────────────┘
             │                 │    ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┘
│            │              │
      ┌──────┴──────┐          │    content process 2
│     │   version   │       │      ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─
      │   module    │          │     ╔═══════════════════════╗      ┌─────────────┐ │
│     └─────────────┘       │      │ ║     WindowGlobal      ╠──────┤  location   │
                               └ ─ ▶ ║    MessageHandler     ║      │   module    │ │
│                           │      │ ╚═══════════════════════╝      └─────────────┘
 ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─        ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┘
```

We can already see here that while RootMessageHandler is connected to both WindowGlobalMessageHandler(s), they are not connected with each other. There are restriction on the way messages can travel on the network both for commands and events, which will be the topic for other documentation pages.
