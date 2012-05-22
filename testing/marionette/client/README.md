<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Marionette Client

[Marionette](https://developer.mozilla.org/en/Marionette) is a 
Mozilla project to enable remote automation in Gecko-based projects,
including desktop Firefox, mobile Firefox, and Boot-to-Gecko.

It utilizes the [remote-debugger](https://wiki.mozilla.org/Platform/JSDebugv2) 
inside Gecko for the transport layer of the Marionette server.  The commands
the Marionette server will eventually implement are based on
Selenium's [JSON Wire Protocol](http://code.google.com/p/selenium/wiki/JsonWireProtocol),
although not all commands are presently implemented, and additional commands
will likely be added.

## Package Files

- client.py:  This is the Marionette socket client; it speaks the same
  socket protocol as the Gecko remote debugger.
- marionette.py:  The Marionette client.  This uses client.py to communicate
  with a server that speaks the Gecko remote debugger protocol.
  This allows Selenium tests to utilize Marionette.
- testserver.py:  A socket server which mimics the remote debugger in
  Gecko, and can be used to test pieces of the Marionette client.
- test_protocol.py:  Tests the Marionette JSON Protocol by using testserver.py.

## Installation

You'll need the ManifestDestiny and MozHttpd packages from Mozbase:

    git clone git://github.com/mozilla/mozbase.git
    cd mozbase
    python setup_development.py

Other than that, there are no special requirements.


## Writing and Running Tests Using Marionette

See [Writing Marionette tests](https://developer.mozilla.org/en/Marionette/Tests),
and [Running Marionette tests](https://developer.mozilla.org/en/Marionette/Running_Tests).

