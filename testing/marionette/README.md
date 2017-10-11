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


Protocol
========

Marionette provides an asynchronous, parallel pipelining user-facing
interface.  Message sequencing limits chances of payload race
conditions and provides a uniform way in which payloads are serialised.

Clients that deliver a blocking WebDriver interface are still
expected to not send further command requests before the response
from the last command has come back, but if they still happen to do
so because of programming error, no harm will be done.  This guards
against [mixing up responses].

Schematic flow of messages:

	               client      server
	                 |            |
	      msgid=1    |----------->|
	                 |  command   |
	                 |            |
	      msgid=2    |<-----------|
	                 |  command   |
	                 |            |
	      msgid=2    |----------->|
	                 |  response  |
	                 |            |
	      msgid=1    |<-----------|
	                 |  response  |
	                 |            |

The protocol consists of a [command] message and the corresponding
[response] message.  A [response] message must always be sent in
reply to a [commmand] message.

This means that the server implementation does not need to send
the reply precisely in the order of the received commands: if it
receives multiple messages, the server may even reply in random order.
It is therefore strongly adviced that clients take this into account
when imlpementing the client end of this wire protocol.

This is required for pipelining messages.  On the server side,
some functions are fast, and some less so.  If the server must
reply in order, the slow functions delay the other replies even if
its execution is already completed.

[mixing up responses]: https://bugzil.la/1207125


Command
-------

The request, or command message, is a four element JSON Array as shown
below, that may originate from either the client- or server remote ends:

	[type, message ID, command, parameters]

  * _type_ must be 0 (integer).  This indicates that the message
    is a [command].

  * _message ID_ is a 32-bit unsigned integer.  This number is
    used as a sequencing number that uniquely identifies a pair of
    [command] and [response] messages.  The other remote part will
    reply with a corresponding [response] with the same message ID.

  * _command_ is a string identifying the RPC method or command
    to execute.

  * _parameters_ is an arbitrary JSON serialisable object.


Response
--------

The response message is also a four element array as shown below,
and must always be sent after receiving a [command]:

	[type, message ID, error, result]

  * _type_ must be 1 (integer).  This indicates that the message is a
    [response].

  * _message ID_ is a 32-bit unsigned integer.  This corresponds
    to the [command]’s message ID.

  * _error_ is null if the command executed correctly.  If the
    error occurred on the server-side, then this is an [error] object.

  * _result_ is the result object from executing the [command], iff
    it executed correctly.  If an error occurred on the server-side,
    this field is null.

The structure of the result field can vary, but is documented
individually for each command.


Error object
------------

An error object is a serialisation of JavaScript error types,
and it is structured like this:

	{
		"error": "invalid session id",
		"message": "No active session with ID 1234",
		"stacktrace": ""
	}

All the fields of the error object are required, so the stacktrace and
message fields may be empty strings.  The error field is guaranteed
to be one of the JSON error codes as laid out by the [WebDriver standard].


Clients
=======

Clients may be implemented in any language that is capable of writing
and receiving data over TCP socket.  A [reference client] is provided.
Clients may be implemented both synchronously and asynchronously,
although the latter is impossible in protocol levels 2 and earlier
due to the lack of message sequencing.


Bugs
====

Bugs are tracked in the `Testing :: Marionette` component.


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
[archive]: http://groups.google.com/group/mozilla.tools.marionette
