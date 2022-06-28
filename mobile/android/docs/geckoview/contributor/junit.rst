.. -*- Mode: rst; fill-column: 80; -*-

====================
Junit Test Framework
====================

GeckoView has `a lot
<https://searchfox.org/mozilla-central/rev/36904ac58d2528fc59f640db57cc9429103368d3/mobile/android/geckoview/src/androidTest/java/org/mozilla/geckoview/test/rule/GeckoSessionTestRule.java>`_
of `custom
<https://searchfox.org/mozilla-central/source/mobile/android/geckoview/src/androidTest/assets/web_extensions/test-support>`_
code that is used to run junit tests. This document is an overview of what this
code does and how it works.

.. contents:: Table of Contents
   :depth: 2
   :local:

Introduction
============

`GeckoView <https://geckoview.dev>`_ is an Android Library that can be used to
embed Gecko, the Web Engine behind Firefox, in applications. It is the
foundation for Firefox on Android, and it is intended to be used to build Web
Browsers, but can also be used to build other types of apps that need to
display Web content.

GeckoView itself has no UI elements besides the Web View and uses Java
interfaces called "delegates" to let embedders (i.e. apps that use GeckoView)
implement UI behavior.

For example, when a Web page's JavaScript code calls ``alert('Hello')`` the
embedder will receive a call to the `onAlertPrompt
<https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.PromptDelegate.html#onAlertPrompt-org.mozilla.geckoview.GeckoSession-org.mozilla.geckoview.GeckoSession.PromptDelegate.AlertPrompt->`_
method of the `PromptDelegate
<https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.PromptDelegate.html>`_
interface with all the information needed to display the prompt.

As most delegate methods deal with UI elements, GeckoView will execute them on
the UI thread for the embedder's convenience.

GeckoResult
-----------

One thing that is important to understand for what follows is `GeckoResult
<https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoResult.html>`_.
``GeckoResult`` is a promise-like object that is used throughout the GeckoView
API, it allows embedders to asynchronously respond to delegate calls and
GeckoView to return results asynchronously. This is especially important for
GeckoView as it never provides synchronous access to Gecko as a design
principle.

For example, when installing a WebExtension in GeckoView, the resulting
`WebExtension
<https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/WebExtension.html>`_
object is returned in a ``GeckoResult``, which is completed when the extension
is fully installed:

.. code:: java

  public GeckoResult<WebExtension> install(...)

To simplify memory safety, ``GeckoResult`` will always `execute callbacks
<https://searchfox.org/mozilla-central/rev/36904ac58d2528fc59f640db57cc9429103368d3/mobile/android/geckoview/src/main/java/org/mozilla/geckoview/GeckoResult.java#740-744>`_
in the same thread where it was created, turning asynchronous code into
single-threaded javascript-style code. This is currently `implemented
<https://searchfox.org/mozilla-central/rev/36904ac58d2528fc59f640db57cc9429103368d3/mobile/android/geckoview/src/main/java/org/mozilla/geckoview/GeckoResult.java#285>`_
using the Android Looper for the thread, which restricts ``GeckoResult`` to
threads that have a looper, like the Android UI thread.

Testing overview
----------------

Given that GeckoView is effectively a translation layer between Gecko and the
embedder, it's mostly tested through integration tests. The vast majority of
the GeckoView tests are of the form:

- Load simple test web page
- Interact with the web page through a privileged JavaScript test API
- Verify that the right delegates are called with the right inputs

and most of the test framework is built around making sure that these
interactions are easy to write and verify.

Tests in GeckoView can be run using the ``mach`` interface, which is used by
most Gecko tests. E.g. to run the `loadUnknownHost
<https://searchfox.org/mozilla-central/rev/36904ac58d2528fc59f640db57cc9429103368d3/mobile/android/geckoview/src/androidTest/java/org/mozilla/geckoview/test/NavigationDelegateTest.kt#186-196>`_
test in ``NavigationDelegateTest`` you would type on your terminal:

.. code:: shell

  ./mach geckoview-junit org.mozilla.geckoview.test.NavigationDelegateTest#loadUnknownHost

Another way to run GeckoView tests is through the `Android Studio IDE
<https://developer.android.com/studio>`_. By running tests this way, however,
some parts of the test framework are not initialized, and thus some tests
behave differently or fail, as will be explained later.

Testing envelope
----------------

Being a library, GeckoView has a natural, stable, testing envelope, namely the
GeckoView API. The vast majority of GeckoView tests only use
publicly-accessible APIs to verify the behavior of the API.

Whenever the API is not enough to properly test behavior, the testing framework
offers targeted "privileged" testing APIs.

Using a restricted, stable testing envelope has proven over the years to be an
effective way of writing consistent tests that don't break upon refactoring.

Testing Environment
-------------------

When run through ``mach``, the GeckoView junit tests run in a similar
environment as mochitests (a type of Web regression tests used in Gecko). They
have access to the mochitest web server at `example.com`, and inherit most of
the testing prefs and profile.

Note the environment will not be the same as mochitests when the test is run
through Android Studio, the prefs will be inherited from the default GeckoView
prefs (i.e. the same prefs that would be enabled in a consumer's build of
GeckoView) and the mochitest web server will not be available.

Tests account for this using the `isAutomation
<https://searchfox.org/mozilla-central/rev/95d8478112eecdd0ee249a941788e03f47df240b/mobile/android/geckoview/src/androidTest/java/org/mozilla/geckoview/test/util/Environment.java#36-38>`_
check, which essentially checks whether the test is running under ``mach`` or
via Android Studio.

Unlike most other junit tests in the wild, GeckoView tests run in the UI
thread. This is done so that the GeckoResult objects are created on the right
thread. Without this, every test would most likely include a lot of blocks that
run code in the UI thread, adding significant boilerplate.

Running tests on the UI thread is achieved by registering a custom ``TestRule``
called `GeckoSessionTestRule
<https://searchfox.org/mozilla-central/rev/36904ac58d2528fc59f640db57cc9429103368d3/mobile/android/geckoview/src/androidTest/java/org/mozilla/geckoview/test/NavigationDelegateTest.kt#186-196>`_,
which, among other things, `overrides the evaluate
<https://searchfox.org/mozilla-central/rev/95d8478112eecdd0ee249a941788e03f47df240b/mobile/android/geckoview/src/androidTest/java/org/mozilla/geckoview/test/rule/GeckoSessionTestRule.java#1307,1312>`_
method and wraps everything into a ``instrumentation.runOnMainSync`` call.

Verifying delegates
===================

As mentioned earlier, verifying that a delegate call happens is one of the most
common assertions that a GeckoView test makes. To facilitate that,
``GeckoSessionTestRule`` offers several ``delegate*`` utilities like:

.. code:: java

  sessionRule.delegateUntilTestEnd(...)
  sessionRule.delegateDuringNextWait(...)
  sessionRule.waitUntilCalled(...)
  sessionRule.forCallbacksDuringWait(...)

These all take an arbitrary delegate object (which may include multiple
delegate implementations) and handle installing and cleaning up the delegate as
needed.

Another set of facilities that ``GeckoSessionTestRule`` offers allow tests to
synchronously ``wait*`` for events, e.g.

.. code:: java

  sessionRule.waitForJS(...)
  sessionRule.waitForResult(...)
  sessionRule.waitForPageStop(...)

These facilities work together with the ``delegate*`` facilities by marking the
``NextWait`` or the ``DuringWait`` events.

As an example, a test could load a page using ``session.loadUri``, wait until
the page has finished loading using ``waitForPageStop`` and then verify that
the expected delegate was called using ``forCallbacksDuringWait``.

Note that the ``DuringWait`` here always refers to the last time a ``wait*``
method was called and finished executing.

The next sections will go into how this works and how it's implemented.

Tracking delegate calls
-----------------------

One thing you might have noticed in the above section is that
``forCallbacksDuringWait`` moves "backward" in time by replaying the delegates
called that happened while the wait was being executed.
``GeckoSessionTestRule`` achieves this by `injecting a proxy object
<https://searchfox.org/mozilla-central/rev/95d8478112eecdd0ee249a941788e03f47df240b/mobile/android/geckoview/src/androidTest/java/org/mozilla/geckoview/test/rule/GeckoSessionTestRule.java#1137>`_
into every delegate, and `proxying every call
<https://searchfox.org/mozilla-central/rev/95d8478112eecdd0ee249a941788e03f47df240b/mobile/android/geckoview/src/androidTest/java/org/mozilla/geckoview/test/rule/GeckoSessionTestRule.java#1091-1092>`_
to the current delegate according to the ``delegate`` test calls.

The proxy delegate `is built
<https://searchfox.org/mozilla-central/rev/95d8478112eecdd0ee249a941788e03f47df240b/mobile/android/geckoview/src/androidTest/java/org/mozilla/geckoview/test/rule/GeckoSessionTestRule.java#1105-1106>`_
using the Java reflection's ``Proxy.newProxyInstance`` method and receives `a
callback
<https://searchfox.org/mozilla-central/rev/95d8478112eecdd0ee249a941788e03f47df240b/mobile/android/geckoview/src/androidTest/java/org/mozilla/geckoview/test/rule/GeckoSessionTestRule.java#1030-1031>`_
every time a method on the delegate is being executed.

``GeckoSessionTestRule`` maintains a list of `"default" delegates
<https://searchfox.org/mozilla-central/rev/95d8478112eecdd0ee249a941788e03f47df240b/mobile/android/geckoview/src/androidTest/java/org/mozilla/geckoview/test/rule/GeckoSessionTestRule.java#743-752>`_
used in GeckoView, and will `use reflection
<https://searchfox.org/mozilla-central/rev/95d8478112eecdd0ee249a941788e03f47df240b/mobile/android/geckoview/src/androidTest/java/org/mozilla/geckoview/test/rule/GeckoSessionTestRule.java#585>`_
to match the object passed into the ``delegate*`` calls to the proxy delegates.

For example, when calling

.. code:: java
  sessionRule.delegateUntilTestEnd(object : NavigationDelegate, ProgressDelegate {})

``GeckoSessionTestRule`` will know to redirect all ``NavigationDelegate`` and
``ProgressDelegate`` calls to the object passed in ``delegateUntilTestEnd``.

Replaying delegate calls
------------------------

Some delegate methods require output data to be passed in by the embedder, and
this requires extra care when going "backward in time" by replaying the
delegate's call.

For example, whenever a page loads, GeckoView will call
``GeckoResult<AllowOrDeny> onLoadRequest(...)`` to know if the load can
continue or not. When replaying delegates, however, we don't know what the
value of ``onLoadRequest`` will be (or if the test is going to install a
delegate for it, either!).

What ``GeckoSessionTestRule`` does, instead, is to `return the default value
<https://searchfox.org/mozilla-central/rev/95d8478112eecdd0ee249a941788e03f47df240b/mobile/android/geckoview/src/androidTest/java/org/mozilla/geckoview/test/rule/GeckoSessionTestRule.java#1092>`_
for the delegate method, and ignore the replayed delegate method return value.
This can be a little confusing for test writers, for example this code `will
not` stop the page from loading:

.. code:: java

  session.loadUri("https://www.mozilla.org")
  sessionRule.waitForPageStop()
  sessionRule.forCallbacksDuringWait(object : NavigationDelegate {
    override fun onLoadRequest(session: GeckoSession, request: LoadRequest) :
        GeckoResult<AllowOrDeny>? {
      // this value is ignored
      return GeckoResult.deny()
    }
  })

as the page has already loaded by the time the ``forCallbacksDuringWait`` call is
executed.

Tracking Waits
--------------

To track when a ``wait`` occurs and to know when to replay delegate calls,
``GeckoSessionTestRule`` `stores
<https://searchfox.org/mozilla-central/rev/95d8478112eecdd0ee249a941788e03f47df240b/mobile/android/geckoview/src/androidTest/java/org/mozilla/geckoview/test/rule/GeckoSessionTestRule.java#1075>`_
the list of delegate calls in a ``List<CallRecord>`` object, where
``CallRecord`` is a class that has enough information to replay a delegate
call. The test rule will track the `start and end index
<https://searchfox.org/mozilla-central/rev/95d8478112eecdd0ee249a941788e03f47df240b/mobile/android/geckoview/src/androidTest/java/org/mozilla/geckoview/test/rule/GeckoSessionTestRule.java#1619>`_
of the last wait's delegate calls and `replay it
<https://searchfox.org/mozilla-central/rev/95d8478112eecdd0ee249a941788e03f47df240b/mobile/android/geckoview/src/androidTest/java/org/mozilla/geckoview/test/rule/GeckoSessionTestRule.java#1697-1724>`_
when ``forCallbacksDuringWait`` is called.

To wait until a delegate call happens, the test rule will first `examine
<https://searchfox.org/mozilla-central/rev/95d8478112eecdd0ee249a941788e03f47df240b/mobile/android/geckoview/src/androidTest/java/org/mozilla/geckoview/test/rule/GeckoSessionTestRule.java#1585>`_
the already executed delegate calls using the call record list described above.
If none of the calls match, then it will `wait for new calls
<https://searchfox.org/mozilla-central/rev/95d8478112eecdd0ee249a941788e03f47df240b/mobile/android/geckoview/src/androidTest/java/org/mozilla/geckoview/test/rule/GeckoSessionTestRule.java#1589>`_
to happen, using ``UiThreadUtils.waitForCondition``.

``waitForCondition`` is also used to implement other type of ``wait*`` methods
like ``waitForResult``, which waits until a ``GeckoResult`` is executed.

``waitForCondition`` runs on the UI thread, and it synchronously waits for an
event to occur. The events it waits for normally execute on the UI thread as
well, so it `injects itself
<https://searchfox.org/mozilla-central/rev/95d8478112eecdd0ee249a941788e03f47df240b/mobile/android/geckoview/src/androidTest/java/org/mozilla/geckoview/test/util/UiThreadUtils.java#145,153>`_
in the Android event loop, checking for the condition after every event has
executed. If no more events remain in the queue, `it posts a delayed 100ms
<https://searchfox.org/mozilla-central/rev/95d8478112eecdd0ee249a941788e03f47df240b/mobile/android/geckoview/src/androidTest/java/org/mozilla/geckoview/test/util/UiThreadUtils.java#136-141>`_
task to avoid clogging the event loop.

Executing Javascript
====================

As you might have noticed from an earlier section, the test rule allows tests
to run arbitrary JavaScript code using ``waitForJS``. The GeckoView API,
however, doesn't offer such an API.

The way ``waitForJS`` and ``evaluateJS`` are implemented will be the focus of
this section.

How embedders run javascript
----------------------------

The only supported way of accessing a web page for embedders is to `write a
built-in WebExtension
<https://firefox-source-docs.mozilla.org/mobile/android/geckoview/consumer/web-extensions.html>`_
and install it. This was done intentionally to avoid having to rewrite a lot of
the Web-Content-related APIs that the WebExtension API offers.

GeckoView extends the WebExtension API to allow embedders to communicate to the
extension by `overloading
<https://searchfox.org/mozilla-central/rev/95d8478112eecdd0ee249a941788e03f47df240b/mobile/android/modules/geckoview/GeckoViewWebExtension.jsm#221>`_
the native messaging API (which is not normally implemented on mobile).
Embedders can register themselves as a `native app
<https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/WebExtension.MessageDelegate.html>`_
and the built-in extension will be able to `exchange messages
<https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/WebExtension.Port.html#postMessage-org.json.JSONObject->`_
and `open ports
<https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/WebExtension.MessageDelegate.html#onConnect-org.mozilla.geckoview.WebExtension.Port->`_
with the embedder.

This is still a controversial topic among smaller embedders, especially solo
developers, and we have discussed internally the possibility to expose a
simpler API to run one-off javascript snippets, similar to what Chromium's
WebView offers, but nothing has been developed so far.

The test runner extension
-------------------------

To run arbitrary javascript in GeckoView, the test runner installs a `support
extension
<https://searchfox.org/mozilla-central/source/mobile/android/geckoview/src/androidTest/assets/web_extensions/test-support>`_.

The test framework then `establishes
<https://searchfox.org/mozilla-central/rev/95d8478112eecdd0ee249a941788e03f47df240b/mobile/android/geckoview/src/androidTest/java/org/mozilla/geckoview/test/rule/GeckoSessionTestRule.java#1827>`_
a port for the background script, used to run code in the main process, and a
port for every window, to be able to run javascript on test web pages.

When ``evaluateJS`` is called, the test framework will send `a message
<https://searchfox.org/mozilla-central/rev/95d8478112eecdd0ee249a941788e03f47df240b/mobile/android/geckoview/src/androidTest/java/org/mozilla/geckoview/test/rule/GeckoSessionTestRule.java#1912>`_
to the extension which then `calls eval
<https://searchfox.org/mozilla-central/rev/95d8478112eecdd0ee249a941788e03f47df240b/mobile/android/geckoview/src/androidTest/assets/web_extensions/test-support/test-support.js#21>`_
on it and returns the `JSON`-stringified version of the result `back
<https://searchfox.org/mozilla-central/rev/95d8478112eecdd0ee249a941788e03f47df240b/mobile/android/geckoview/src/androidTest/java/org/mozilla/geckoview/test/rule/GeckoSessionTestRule.java#1952-1956>`_
to the test framework.

The test framework also supports promises with `evaluatePromiseJS
<https://searchfox.org/mozilla-central/rev/95d8478112eecdd0ee249a941788e03f47df240b/mobile/android/geckoview/src/androidTest/java/org/mozilla/geckoview/test/rule/GeckoSessionTestRule.java#1888>`_.
It works similarly to ``evaluateJS`` but instead of returning the stringified
value, it `sets
<https://searchfox.org/mozilla-central/rev/95d8478112eecdd0ee249a941788e03f47df240b/mobile/android/geckoview/src/androidTest/java/org/mozilla/geckoview/test/rule/GeckoSessionTestRule.java#1879>`_
the return value of the ``eval`` call into the ``this`` object, keyed by a
randomly-generated UUID.

.. code:: java

  this[uuid] = eval(...)

``evaluatePromiseJS`` then returns an ``ExtensionPromise`` Java object which
has a ``getValue`` method on it, which will essentially execute `await
this[uuid]
<https://searchfox.org/mozilla-central/rev/95d8478112eecdd0ee249a941788e03f47df240b/mobile/android/geckoview/src/androidTest/java/org/mozilla/geckoview/test/rule/GeckoSessionTestRule.java#1883-1885>`_
to get the value from the promise when needed.

Beyond executing javascript
---------------------------

A natural way of breaking the boundaries of the GeckoView API is to run a
so-called "experiment extension". Experiment extensions have access to the full
Gecko front-end, which is written in JavaScript, and don't have limits on what
they can do. Experiment extensions are essentially what old add-ons used to be
in Firefox, very powerful and very dangerous.

The test runner uses experiments to offer `privileged APIs
<https://searchfox.org/mozilla-central/rev/95d8478112eecdd0ee249a941788e03f47df240b/mobile/android/geckoview/src/androidTest/assets/web_extensions/test-support/test-api.js>`_
to tests like ``setPref`` or ``getLinkColor`` (which is not normally available
to websites for privacy concerns).

Each privileged API is exposed as an `ordinary Java API
<https://searchfox.org/mozilla-central/rev/95d8478112eecdd0ee249a941788e03f47df240b/mobile/android/geckoview/src/androidTest/java/org/mozilla/geckoview/test/rule/GeckoSessionTestRule.java#2101>`_
and the test framework doesn't offer a way to run arbitrary chrome code to
discourage developers from relying too much on implementation-dependent
privileged code.
