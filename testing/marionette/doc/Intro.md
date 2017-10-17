Introduction to Marionette
==========================

Marionette is an automation driver for Mozilla's Gecko engine.
It can remotely control either the UI or the internal JavaScript of
a Gecko platform, such as Firefox.  It can control both the chrome
(i.e. menus and functions) or the content (the webpage loaded inside
the browsing context), giving a high level of control and ability
to replicate user actions. In addition to performing actions on the
browser, Marionette can also read the properties and attributes of
the DOM.

If this sounds similar to [Selenium/WebDriver] then you're
correct! Marionette shares much of the same ethos and API as
Selenium/WebDriver, with additional commands to interact with
Gecko's chrome interface.  Its goal is to replicate what Selenium
does for web content: to enable the tester to have the ability to
send commands to remotely control a user agent.

[Selenium/WebDriver]: https://dvcs.w3.org/hg/webdriver/raw-file/tip/webdriver-spec.html


How does it work?
=================

Marionette consists of two parts: a server which takes requests and
executes them in Gecko, and a client.  The client sends commands to
the server and the server executes the command inside the browser.


When would I use it?
====================

If you want to perform UI tests with browser chrome or content,
Marionette is the tool you're looking for!  You can use it to
control either web content, or Firefox itself.

A test engineer would typically import the Marionette client package
into their test framework, import the classes and use the class
functions and methods to control the browser.  After controlling
the browser, Marionette can be used to return information about
the state of the browser which can then be used to validate that
the action was performed correctly.


Using Marionette
================

Marionette combines a gecko component (the Marionette server) with an
outside component (the Marionette client), which drives the tests.
The Marionette server ships with Firefox, and to use it you will
need to download a Marionette client or use the in-tree client.

  * [Download and setup the Python client for Marionette][2]
  * [Run Tests with Python][3] – How to run tests using the
    Python client
  * You might want to experiment with [using Marionette interactively
    at a Python command prompt][3]
  * Start [writing and running][4] tests
  * Tips on [debugging][5] Marionette code
  * [Get a Build][6] – Instructions on how to get a Marionette-enabled
    build of Firefox
  * [Download and setup the Marionette JS client][7]
  * [Protocol definition][8]

[1] https://developer.mozilla.org/en-US/docs/Mozilla/QA/Marionette/WebDriver
[2] http://marionette-client.readthedocs.io/en/latest/
[3] http://marionette-client.readthedocs.io/en/latest/interactive.html
[4] ./PythonTests.md
[5] ./Debugging.md
[6] https://developer.mozilla.org/en-US/docs/Marionette/Builds
[7] https://github.com/mozilla-b2g/marionette_js_client
[8] ./Protocol.md


Bugs
====

Please file any bugs you may find in the `Testing :: Marionette`
component in Bugzilla.  You can view a [list of current bugs]
to see if your problem is already being addressed.

[list of current bugs]: https://bugzilla.mozilla.org/buglist.cgi?product=Testing&component=Marionette&resolution=---&list_id=1844713
