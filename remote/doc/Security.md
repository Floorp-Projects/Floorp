Security aspects of the Remote Agent
====================================

The Remote Agent is not a web-facing feature and as such has different
security characteristics than traditional web platform APIs.  The
primary consumers are out-of-process programs that connect to the
agent via a remote protocol, but can theoretically be extended to
facilitate browser-local clients communicating over IPDL.


Design considerations
---------------------

The Remote Agent allows consumers to interface with Firefox through
an assorted set of domains for inspecting the state and controlling
execution of documents running in web content, injecting arbitrary
scripts to documents, do browser service instrumentation, simulation
of user interaction for automation purposes, and for subscribing
to updates in the browser such as network- and console logs.

The remote interfaces are served over an HTTP wire protocol, by a
server listener hosted in the Firefox binary.  This can only be
started by passing the `--remote-debugging-port`
flag.  Connections are by default restricted to loopback devices
(such as localhost and 127.0.0.1), but this can be overridden with
the `remote.force-local` preference.

Since the Remote Agent is not an in-document web feature, the
security concerns we have for this feature are essentially different
to other web platform features.  The primary concern is that the
HTTPD is not spun up without passing one of the command-line flags.
It is out perception that if a malicious user has the capability
to execute arbitrary shell commands, there is little we can do to
prevent the browser being turned into an evil listening device.


User privacy concerns
---------------------

There are no user privacy concerns beyond the fact that the offered
interfaces will give the client access to all browser internals,
and thereby follows all browser-internal secrets.


How the Remote Agent works
--------------------------

When the `--remote-debugging-port` flag is used,
it spins up an HTTPD on the desired port, or defaults to
localhost:9222.  The HTTPD serves WebSocket connections via
`nsIWebSocket.createServerWebSocket` that clients connect to in
order to give the agent remote instructions.

The `remote.force-local` preference controls whether the HTTPD
accepts connections from non-loopback clients.  System-local loopback
connections are the default:

	    if (Preferences.get(FORCE_LOCAL) && !LOOPBACKS.includes(host)) {
	      throw new Error("Restricted to loopback devices");
	    }

The Remote Agent implements a large subset of the Chrome DevTools
Protocol (CDP).  This protocol allows a client to:

  - take control over the user session for automation purposes, for
    example to simulate user interaction such as clicking and typing;

  - instrument the browser for analytical reasons, such as intercepting
    network traffic;

  - and extract information from the user session, including cookies
    and local strage.

There are no web-exposed features in the Remote Agent whatsoever.


Security model
--------------

It shares the same security model as DevTools and Marionette, in
that there is no other mechanism for enabling the Remote Agent than
by passing a command-line flag.

It is our assumption that if an attacker has shell access to the
user account, there is little we can do to prevent secrets from
being accessed or leaked.

The Remote Agent is available on all release channels.
The [security review] was completed in November 2019.


[security review]: https://bugzilla.mozilla.org/show_bug.cgi?id=1542229
