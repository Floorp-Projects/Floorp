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
- selenium_proxy.py:  Acts as a remote driver for Selenium test runners.
  This code translates the Selenium 
  [JSON Wire Protocol](http://code.google.com/p/selenium/wiki/JsonWireProtocol)
  to the [Marionette JSON Protocol](https://wiki.mozilla.org/Auto-tools/Projects/Marionette/JSON_Protocol).
  This allows Selenium tests to utilize Marionette.
- testserver.py:  A socket server which mimics the remote debugger in
  Gecko, and can be used to test pieces of the Marionette client.
- test_protocol.py:  Tests the Marionette JSON Protocol by using testserver.py.
- test_selenium.py:  Tests the Selenium proxy by using testserver.py.

## Installation

You'll need the ManifestDestiny and MozHttpd packages from Mozbase:

    git clone git://github.com/mozilla/mozbase.git
    cd mozbase
    python setup_development.py

Other than that, there are no special requirements, unless you're using the Selenium proxy, in which
case you'll need to install the Selenium Python bindings using:

    pip install selenium

## Writing and Running Tests Using Marionette

See [Writing Marionette tests](https://developer.mozilla.org/en/Marionette/Tests),
and [Running Marionette tests](https://developer.mozilla.org/en/Marionette/Running_Tests).

