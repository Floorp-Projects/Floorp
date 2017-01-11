.. -*- Mode: rst; fill-column: 80; -*-

=======================================================
 The architecture of the Fennec Webpush implementation
=======================================================

Overview of the Push API
========================

The *Push API* is a Web API that allows web applications to "wake up" the User
Agent (the browser, Fennec), even when the application is not visible (or even
loaded in a tab).  The Push API can only be used by secure sites that register a
Service Worker.

There are four major components in Push:

1. A *web application*;
2. The User Agent (Fennec);
3. An *app server* associated to the web application;
4. A *push service backend*.

These are listed roughly in the order that they appear in a successful push
message.  First, the web application registers a Service Worker and requests a
*push subscription*.  Fennec arranges a push channel and returns a subscription,
including a User-Agent-specific *push endpoint* URL to the web application.  The
web application then provides that push endpoint URL to its app server.  When
the app server wishes to push a message to the web application, it posts the
message and some additional meta-data to the provided push endpoint URL.  The
message is received by the *push service backend* and delivered by some
unspecified out-of-band mechanism to the User Agent.

Two notes on terminology
------------------------

"Push notifications" mean many things to many people.  In this system, "push
notifications" may or may not notify the user.  Therefore, we use the term "push
messages" exclusively.  This avoids confusing the push system with the "Web
Notification" API, which provides the familiar pop-up and system notification
based user interface.

Throughout, we use Fennec to refer to the Java-and-Android application shell,
and Gecko to refer to the embedded Gecko platform.

Overview of the Fennec Push implementation
==========================================

Fennec uses the Google Cloud Messaging (GCM) service to deliver messages to the
User Agent.  Fennec registers for Google Cloud Messaging directly, like any
other Android App; however, consumers do not interact with GCM directly.  To
provide a uniform interface to all consumers across all implementations, Fennec
intermediates through the Mozilla-specific *autopush* service.  Autopush
maintains User Agent identification and authentication, provides per-web
application messaging channels, and bridges unauthenticated push messages to the
GCM delivery queue.

The Fennec Push implementation is designed to address the following technical
challenge: **GCM events, including incoming push messages, can occur when Gecko
is not running**. In case of an incoming push message, if Gecko is not running
Fennec will request startup of necessary Gecko services and queue incoming
push messages in the meantime. Once services are running, messages are sent over.

It's worth noting that Fennec uses push to implement internal functionality like
Sync and Firefox Accounts, and that these background services are *not* tied to
Gecko being available.

Therefore, the principal constraints and requirements are:

1) Fennec must be able to service GCM events, including incoming push messages,
independently of Gecko.

2) Gecko must be able to maintain push subscriptions across its entire
life-cycle.

3) Fennec must be able to use push messages for non-Gecko purposes independently
of Gecko.

Significant previous experience building Fennec background services has shown
that configuring such services across the Gecko-Fennec interface is both
valuable and difficult.  Therefore, we add the following requirement:

4) Gecko must own the push configuration details where appropriate and possible.

We explicitly do not care to support push messages across multiple processes. This
will matter more in a post-e10s-on-Android world.

Push component architecture
===========================

Fennec components
-----------------

The two major components are the `PushManager` and the associated
`PushManagerStorage`.  The `PushManager` interacts with the GCM system on the
device and the autopush service, updating the `PushManagerStorage` as the system
state changes.

There is a unique `PushManagerStorage` instance per-App that may only be
accessed by the main process.  The `PushManagerStorage` maintains two mappings.
The first is a one-to-one mapping from a Gecko profile to a `PushRegistration`:
a datum of User Agent state.  The `PushManager` maintains each profile's
registration across Android life-cycle events, Gecko events, and GCM events.
Each `PushRegistration` includes:

* autopush server configuration details;
* debug settings;
* profile details;
* access tokens and invalidation timestamps.

The second mapping is a one-to-many mapping from push registrations to
`PushSubscription` instances.  A push subscription corresponds to a unique push
message channel from the autopush server to Fennec.  Each `PushSubscription`
includes:

* a Fennec service identifier, one of "webpush" or "fxa";
* an associated Gecko profile;
* a unique channel identifier.

The `PushManager` uses the `PushSubscription` service and profile maintained in
the `PushManagerStorage` to determine how to deliver incoming GCM push messages.

Each `PushRegistration` corresponds to a unique *uaid* (User-Agent ID) on the
autopush server.  Each *uaid* is long-lived; a healthy client will maintain the
same *uaid* until the client's configuration changes or the service expires the
registration due to inactivity or an unexpected server event.  Each
`PushSubscription` is associated to a given *uaid* and correponds to a unique
(per-*uaid*) *chid* (Channel ID) on the autopush server.  An individual *chid*
is potentially long-lived, but clients must expect the service to expire *chid*s
as part of regular maintainence.  The `PushManager` uses an `AutopushClient`
instance to interact with the autopush server.

Between the `PushManager`, the `PushManagerStorage`, and assorted GCM event
broadcast receivers, push messages that do not target Gecko can be implemented.

Gecko components
----------------

The Gecko side of the architecture is implemented in JavaScript by the
`PushServiceAndroidGCM.jsm` module.  This registers a PushService, like the Web
Socket and HTTP2 backed services, which simply delegates to the Fennec
`PushManager` using `Messaging.jsm` and friends.

There are some complications: first, Gecko must maintain the autopush
configuration; and second, it is possible for the push system to change while
Gecko is not running.  Therefore, the communication is bi-directional
throughout, so that Gecko can react to Fennec events after-the-fact.