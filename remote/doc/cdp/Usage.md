# Usage

When using the CDP-based Remote Agent in Firefox, there are
three different programs/components running simultaneously:

* the __client__, being the out-of-process script or library
  (such as Puppeteer) or web inspector frontend you use to control
  and retrieve information out of Firefox;

* the __agent__ that the client connects to which is an HTTPD living
  inside Firefox, facilitating communication between clients
  and targets;

* and the __target__, which is the web document being debugging.

Since Firefox 86 the Remote Agent ships in all Firefox releases by default.

To check if your Firefox binary has the Remote Agent enabled, you
can look in its help message for this:

```shell
% ./firefox -h
…
  --remote-debugging-port [<port>] Start the Firefox Remote Agent, which is
                     a low-level debugging interface based on the CDP protocol.
                     Defaults to listen on localhost:9222.
…
```

When used, the Remote Agent will start an HTTP server and print a
message on stderr with the location of the main target’s WebSocket
listener:

```shell
% firefox --remote-debugging-port
DevTools listening on ws://localhost:9222/devtools/browser/7b4e84a4-597f-4839-ac6d-c9e86d16fb83
```

The argument takes an optional `port` as value:

You can also instruct the Remote Agent to bind to a particular port on
your system.  Therefore the argument accepts an optional value, which means
that `firefox --remote-debugging-port=9989`
will bind the HTTPD to port `9989`:

```shell
% firefox --remote-debugging-port 9989
DevTools listening on ws://localhost:9989/devtools/browser/b49481af-8ad3-9b4d-b1bf-bb0cdb9a0620
```

If the value is missing the default port `9222` will be used.

When you ask the Remote Agent to listen on port 0,
the system will atomically allocate an arbitrary free port:

```shell
% firefox --remote-debugging-port 0
DevTools listening on ws://localhost:59982/devtools/browser/a12b22a9-1b8b-954a-b81f-bd31552d3f1c
```

Allocating an atomic port can be useful if you want to avoid race
conditions.  The atomically allocated port will be somewhere in the
ephemeral port range, which varies depending on your system and
system configuration, but is always guaranteed to be free thus
eliminating the risk of binding to a port that is already in use.
