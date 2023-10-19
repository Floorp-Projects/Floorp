Remote Agent overall architecture
=================================

This document will cover the Remote Agent architecture by following the sequence of steps needed to start the agent, connect a client and debug a target.

Remote Agent startup
--------------------

Everything starts with the `RemoteAgent` implementation, which handles command line
arguments (--remote-debugging-port) to eventually
start a server listening on the TCP port 9222 (or the one specified by the command line).
The browser target websocket URL will be printed to stderr.
To do that this component glue together three main high level components:

  * `server/HTTPD`
    This is a copy of httpd.js, from /netwerk/ folder. This is a JS implementation of an http server.
    This will be used to implement the various http endpoints of CDP.
    There is a few static URL implemented by `JSONHandler` and one dynamic URL per target.

  * `cdp/JSONHandler`
    This implements the following three static http endpoints:
    * /json/version:
      Returns information about the runtime as well as the url of the browser target websocket url.
    * /json/list:
      Returns a list of all debuggable targets with, for each, their dynamic websocket URL.
      For now it only reports tabs, but will report workers and addons as soon as we support them.
      The main browser target is the only one target not listed here.
    * /json/protocol:
      Returns a big dictionary describing the supported protocol.
      This is currently hard coded and returns the full CDP protocol schema, including APIs we don’t support.
      We have a future intention to fix this and report only what Firefox implements.
    You can connect to these websocket URL in order to debug things.

  * `cdp/targets/TargetList`
    This component is responsible of maintaining the list of all debuggable targets.
    For now it can be either:
    * The main browser target
      A special target which allows to inspect the browser, but not any particular tab.
      This is implemented by `cdp/targets/MainProcessTarget` and is instantiated on startup.
    * Tab targets
      Each opened tab will have a related `cdp/targets/TabTarget` instantiated on their opening,
      or on server startup for already opened ones.
    Each target aims at focusing on one particular context. This context is typically running in one
    particular environment. This can be a particular process or thread.
    In the future, we will most likely support targets for workers and add-ons.
    All targets inherit from `cdp/targets/Target`.

Connecting to Websocket endpoints
---------------------------------

Each target's websocket URL will be registered as a HTTP endpoint via `server/HTTPD:registerPathHandler` (This registration is done from `RemoteAgentParentProcess:#listen`).
Once a HTTP request happens, `server/HTTPD` will call the `handle` method on the object passed to `registerPathHandler`.
For static endpoints registered by `JSONHandler`, this will call `JSONHandler:handle` and return a JSON string as http body.
For target's endpoint, it is slightly more complicated as it requires a special handshake to morph the HTTP connection into a WebSocket one.
The WebSocket is then going to be long lived and be used to inspect the target over time.
When a request is made to a target URL, `cdp/targets/Target:handle` is called and:

  * delegate the complex HTTP to WebSocket handshake operation to `server/WebSocketHandshake:upgrade`
    In return we retrieve a WebSocket object.

  * hand over this WebSocket to `server/WebSocketTransport`
    and get a transport object in return. The transport implements a basic JSON stream over WebSocket. With that, you can send and receive JSON objects over a WebSocket connection.

  * hand over the transport to a freshly instantiated `Connection`
    The Connection has two goals:
    * Interpret incoming CDP packets by reading the JSON object attribute (`id`, `method`, `params` and `sessionId`)
      This is done in `Connection:onPacket`.
    * Format outgoing CDP packets by writing the right JSON object for command response (`id`, `result` and `sessionId`) and events (`method`, `params` and `sessionId`)
    * Redirect CDP packet from/to the right session.
    A connection may have more than one session attached to it.

  * instantiate the default session
    The session is specific to each target kind and all of them inherit from `cdp/session/Session`.
    For example, tabs targets uses `cdp/session/TabSession` and the main browser target uses `cdp/session/MainProcessSession`.
    Which session class is used is defined by the Target subclass’ constructor, which pass a session class reference to `cdp/targets/Target:constructor`.
    A session is mostly responsible of accommodating the eventual cross process/cross thread aspects of the target.
    The code we are currently describing (`cdp/targets/Target:handle`) is running in the parent process.
    The session class receive CDP commands from the connection and first try to execute the Domain commands in the parent process.
    Then, if the target actually runs in some other context, the session tries to forward this command to this other context, which can be a thread or a process.
    Typically, the `cdp/sessions/TabSession` forward the CDP command to the content process where the tab is running.
    It also redirects back the command response as well as Domain events from that process back to the parent process in order to
    forward them to the connection.
    Sessions will be using the `DomainCache` class as a helper to manage a list of Domain implementations in a given context.

Debugging additional Targets
----------------------------

From a given connection you can know about the other potential targets.
You typically do that via `Target.setDiscoverTargets()`, which will emit `Target.targetCreated` events providing a target ID.
You may create a new session for the new target by handing the ID to `Target.attachToTarget()`, which will return a session ID.
"Target" here is a reference to the CDP Domain implemented in `cdp/domains/parent/Target.jsm`. That is different from `cdp/targets/Target`
class which is an implementation detail of the Remote Agent.

Then, there is two ways to communicate with the other targets:

  * Use `Target.sendMessageToTarget()` and `Target.receivedMessageFromTarget`
    You will manually send commands via the `Target.sendMessageToTarget()` command and receive command's response as well as events via `Target.receivedMessageFromTarget`.
    In both cases, a session ID attribute is passed in the command or event arguments in order to select which additional target you are communicating with.

  * Use `Target.attachToTarget({ flatten: true })` and include `sessionId` in CDP packets
    This requires a special client, which will use the `sessionId` returned by `Target.attachToTarget()` in order to spawn a distinct client instance.
    This client will reuse the same WebSocket connection, but every single CDP packet will contain an additional `sessionId` attribute.
    This helps distinguish packets which relate to the original target as well as the multiple additional targets you may attach to.

In both cases, `Target.attachToTarget()` is special as it will spawn `cdp/session/TabSession` for the tab you are attaching to.
This is the codepath creating non-default session. The default session is related to the target you originally connected to,
so that you don't need any ID for this one. When you want to debug more than one target over a single connection
you need additional sessions, which will have a unique ID.
`Target.attachToTarget` will compute this ID and instantiate a new session bound to the given target.
This additional session will be managed by the `Connection` class, which will then redirect CDP packets to the
right session when you are using flatten session.

Cross Process / Layers
----------------------

Because targets may runs in different contexts, the Remote Agent code runs in different processes.
The main and startup code of the Remote Agent code runs in the parent process.
The handling of the command line as well as all the HTTP and WebSocket work is all done in the parent process.
The browser target is also all implemented in the parent process.
But when it comes to a tab target, as the tab runs in the content process, we have to run code there as well.
Let's start from the `cdp/sessions/TabSession` class, which has already been described.
We receive here JSON packets from the WebSocket connection and we are in the parent process.
In this class, we route the messages to the parent process domains first.
If there is no implementation of the domain or the particular method,
we forward the command to a `cdp/session/ContentProcessSession` which runs in the tab's content process.
These two Session classes will interact with each other in order to forward back the returned value
of the method we just called, as well as piping back any event being sent by a Domain implemented in any
of the two processes.

Organizational chart of all the classes
----------------------------------------
```
            ┌─────────────────────────────────────────────────┐
            │                                                 │
          1 ▼                                                 │
    ┌───────────────┐     1 ┌───────────────┐     1..n┌───────────────┐
    │  RemoteAgent  │──────▶│  HttpServer   │◀───────▶│  JsonHandler  │
    └───────────────┘       └───────────────┘ 1       └───────────────┘
            │
            │
            │              1 ┌────────────────┐ 1
            └───────────────▶│  TargetList    │◀─┐
                             └────────────────┘  │
                                    │            │
                                    ▼ 1..n       │
                             ┌────────────┐      │
           ┌─────────────────│ Target  [1]│      │
           │                 └────────────┘      │
           │                        ▲ 1          │
           ▼ 1..n                   │            │
    ┌────────────┐       1..n┌────────────┐      │
    │ Connection │◀─────────▶│ Session [2]│──────┘
    └────────────┘ 1         └────────────┘
           │                      1 ▲
           │                        │
           ▼ 1                      ▼ 1
┌────────────────────┐       ┌──────────────┐         1..n┌────────────┐
│ WebSocketTransport │       │  DomainCache | │──────────▶│ Domain  [3]│
└────────────────────┘       └──────────────┘             └────────────┘
```
 [1] Target is inherited by TabTarget and MainProcessTarget.
 [2] Session is inherited by TabSession and MainProcessSession.
 [3] Domain is inherited by Log, Page, Browser, Target.... i.e. all domain implementations. From both cdp/domains/parent and cdp/domains/content folders.
