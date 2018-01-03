Marionette
==========

Marionette is the remote protocol that lets OOP programs communicate
with, instrument, and control Gecko.


Description
===========

Marionette is an automation driver for Mozilla’s Gecko engine.
It can remotely control either the UI or the internal JavaScript of
Gecko-based browsers, such as Firefox and Fennec.  It can control
both the chrome and the content document, giving a high level of
control and ability to replicate user interaction.  In addition
to performing actions on the browser, Marionette can also ready
properties and attributes of the DOM.


Usage
=====

Marionette can be activated by passing the `-marionette` flag.
To start Firefox with the remote protocol turned on:

	% firefox -marionette
	…
	1491228343089	Marionette	INFO	Listening on port 2828

This binds to a TCP socket, over which [clients] can communicate
with Marionette using the [protocol].


Clients
=======

Clients may be implemented in any language that is capable of writing
and receiving data over TCP socket.  A [reference client] is provided.
Clients may be implemented both synchronously and asynchronously,
although the latter is impossible in protocol levels 2 and earlier
due to the lack of message sequencing.


Contributing
============

If you want to help improve Marionette, we have more information in
<CONTRIBUTING.md>.  You will find additional development documentation at
<https://firefox-source-docs.mozilla.org/testing/marionette/marionette>.


Communication
=============

The mailing list for discussion is tools-marionette@lists.mozilla.org
([subscribe], [archive]).  If you prefer real-time chat, there
is often someone in the #ateam IRC channel on irc.mozilla.org.
Don’t ask if you can ask a question, just ask, and please wait
for an answer as we might not be in your timezone.


[clients]: #clients
[protocol]: #protocol
[command]: #command
[response]: #response
[error]: #error-object
[WebDriver standard]: https://w3c.github.io/webdriver/webdriver-spec.html#handling-errors
[reference client]: client/
[subscribe]: https://lists.mozilla.org/listinfo/tools-marionette
[archive]: https://groups.google.com/group/mozilla.tools.marionette
