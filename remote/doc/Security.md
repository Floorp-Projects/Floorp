Security aspects of the remote agent
====================================

The remote agent is not a web-facing feature and as such has different
security characteristics than traditional web platform APIs.  The
primary consumers are out-of-process programs that connect to the
agent via a remote protocol, but can theoretically be extended to
facilitate browser-local clients communicating over IPDL.


Design considerations
---------------------

The remote agent allows consumers to interface with Firefox through
an assorted set of domains for inspecting the state and controlling
execution of documents running in web content, injecting arbitrary
scripts to documents, do browser service instrumentation, simulation
of user interaction for automation purposes, and for subscribing
to updates in the browser such as network- and console logs.

The remote interfaces are served over an HTTP wire protocol, by a
server listener hosted in the Firefox binary.  This can only be
started by passing the `--remote-debugger` or `--remote-debugging-port`
flags.  Connections are by default restricted to loopback devices
(such as localhost and 127.0.0.1), but this can be overridden with
the `remote.force-local` preference.

The feature as a whole is guarded behind the `remote.enabled`
preference.  This preference serves as a way to gate the remote
agent component through release channels, and potentially for
remotely disabling the remote agent through Normandy if the need
should arise.

Since the remote agent is not an in-document web feature, the
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


How the remote agent works
--------------------------

When the `--remote-debugger` or `--remote-debugging-port` flags are
used, it spins up an HTTPD on the desired port, or defaults to
localhost:9222.  The HTTPD serves WebSocket connections via
`nsIWebSocket.createServerWebSocket` that clients connect to in
order to give the agent remote instructions.

The `remote.force-local` preference controls whether the HTTPD
accepts connections from non-loopback clients.  System-local loopback
connections are the default:

	    if (Preferences.get(FORCE_LOCAL) && !LOOPBACKS.includes(host)) {
	      throw new Error("Restricted to loopback devices");
	    }

The remote agent implements a large subset of the Chrome DevTools
Protocol (CDP).  This protocol allows a client to:

  - take control over the user session for automation purposes, for
    example to simulate user interaction such as clicking and typing;

  - instrument the browser for analytical reasons, such as intercepting
    network traffic;

  - and extract information from the user session, including cookies
    and local strage.

There are no web-exposed features in the remote agent whatsoever.


Security model
--------------

It shares the same security model as DevTools and Marionette, in
that there is no other mechanism for enabling the remote agent than
by passing a command-line flag.

It is our assumption that if an attacker has shell acccess to the
user account, there is little we can do to prevent secrets from
being accessed or leaked.

The preference `remote.enabled` defaults to false in Nightly, and
it is the purpose to flip this to true to ship the remote agent for
Firefox Nightly users following [a security review].

[a security review]: https://bugzilla.mozilla.org/show_bug.cgi?id=1542229
