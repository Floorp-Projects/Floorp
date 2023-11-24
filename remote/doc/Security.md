# Security aspects of the Remote Agent

The Remote Agent is not a web-facing feature and as such has different
security characteristics than traditional web platform APIs.  The
primary consumers are out-of-process programs that connect to the
agent via a remote protocol, but can theoretically be extended to
facilitate browser-local clients communicating over IPDL.

## Design considerations

The Remote Agent allows consumers to interface with Firefox through
an assorted set of domains for inspecting the state and controlling
execution of documents running in web content, injecting arbitrary
scripts to documents, do browser service instrumentation, simulation
of user interaction for automation purposes, and for subscribing
to updates in the browser such as network- and console logs.

The remote interfaces are served over an HTTP wire protocol, by a
server listener hosted in the Firefox binary.  This can only be
started by passing the `--remote-debugging-port`
flag.  Connections are restricted to loopback devices
(such as localhost and 127.0.0.1).

Since the Remote Agent is not an in-document web feature, the
security concerns we have for this feature are essentially different
to other web platform features.  The primary concern is that the
HTTPD is not spun up without passing one of the command-line flags.
It is out perception that if a malicious user has the capability
to execute arbitrary shell commands, there is little we can do to
prevent the browser being turned into an evil listening device.

## User privacy concerns

There are no user privacy concerns beyond the fact that the offered
interfaces will give the client access to all browser internals,
and thereby follows all browser-internal secrets.

## How the Remote Agent works

When the `--remote-debugging-port` flag is used,
it spins up an HTTPD on the desired port, or defaults to
localhost:9222.  The HTTPD serves WebSocket connections via
`nsIWebSocket.createServerWebSocket` that clients connect to in
order to give the agent remote instructions. Hereby the HTTPD only
accepts system-local loopback connections from clients:

```javascript
if (!LOOPBACKS.includes(host)) {
  throw new Error("Restricted to loopback devices");
}
```

The Remote Agent implements a large subset of the Chrome DevTools
Protocol (CDP).  This protocol allows a client to:

* take control over the user session for automation purposes, for
  example to simulate user interaction such as clicking and typing;

* instrument the browser for analytical reasons, such as intercepting
  network traffic;

* and extract information from the user session, including cookies
  and local storage.

There are no web-exposed features in the Remote Agent whatsoever.

## Security model

It shares the same security model as DevTools and Marionette, in
that there is no other mechanism for enabling the Remote Agent than
by passing a command-line flag.

It is our assumption that if an attacker has shell access to the
user account, there is little we can do to prevent secrets from
being accessed or leaked.

The Remote Agent is available on all release channels.

## Remote Hosts and Origins

By default RemoteAgent only accepts connections with no `Origin` header and a
`Host` header set to an IP address or a localhost loopback address.

Other `Host` or `Origin` headers can be allowed by starting Firefox with the
`--remote-allow-origins` and `--remote-allow-hosts` arguments:

* `--remote-allow-hosts` expects a comma separated list of hostnames

* `--remote-allow-origins` expects a comma separated list of origins

Note: Users are strongly discouraged from using the Remote Agent in a way that
allows it to be accessed by untrusted hosts e.g. by binding it to a publicly
routeable interface.

The Remote Agent does not provide message encryption, which means that all
protocol messages are subject to eavesdropping and tampering. It also does not
provide any authentication system. This is acceptable in an isolated test
environment, but not to be used on an untrusted network such as the internet.
People wishing to provide remote access to Firefox sessions via the Remote Agent
must provide their own encryption, authentication, and authorization.

## Security reviews

More details can be found in the security reviews conducted for Remote Agent and
WebDriver BiDi:

* [Remote Agent security review] (November 2019)

* [WebDriver BiDi security review] (April 2022)

[Remote Agent security review]: https://bugzilla.mozilla.org/show_bug.cgi?id=1542229
[WebDriver BiDi security review]: https://bugzilla.mozilla.org/show_bug.cgi?id=1753997
