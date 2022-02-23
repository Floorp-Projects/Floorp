.. -*- Mode: rst; fill-column: 80; -*-

======================
GeckoView architecture
======================

.. contents:: Table of Contents
   :depth: 2
   :local:

Introduction
============

*Gecko* is a Web engine developed by Mozilla and used to power Firefox on
various platforms. A Web engine is roughly comprised of a JavaScript engine, a
Rendering engine, HTML parser, a Network stack, various media encoders, a
Graphics engine, a Layout engine and more.

Code that is part of a browser itself is usually referred to as "chrome" code
(from which the popular Chrome browser takes its name) as opposed to code part
of a Web site, which is usually referred to "content" code or content Web page.

*GeckoView* is an Android library that can be used to embed Gecko into Android
apps. Android apps that embed Gecko this way are usually referred to by
"embedders" or simply "apps".

GeckoView powers all currently active Mozilla browsers on Android, like Firefox
for Android and Firefox Focus.

API
===

The following sections describe parts of the GeckoView API that are public and
exposed to embedders.

   |api-diagram|

Overall tenets
--------------

GeckoView is an opinionated library that contains a minimal UI and makes no
assumption about the type of app that is being used by. Its main consumers
inside Mozilla are browsers, so a lot of features of GeckoView are geared
towards browsers, but there is no assumption that the embedder is actually a
browser (e.g. there is no concept of "tab" in GeckoView).

The GeckoView API tries to retain as little data as possible, delegating most
data storage to apps. Notable exceptions to this rule are: permissions,
extensions and cookies.

View, Runtime and Session
-------------------------

    |view-runtime-session|

There are three main classes in the GeckoView API:

- ``GeckoRuntime`` represents an instance of Gecko running in an app. Normally,
  apps have only one instance of the runtime which lives for as long as the app
  is alive. Any object in the API that is not specific to a *session*
  (more to this later) is usually reachable from the runtime.
- ``GeckoSession`` represents a web site *instance*. You can think of it as a
  *tab* in a browser or a Web view in an app. Any object related to the
  specific session will be reachable from this object. Normally, embedders
  would have many instances of ``GeckoSession`` representing each tab that is
  currently open. Internally, a session is represented as a "window" with one
  single tab in it.
- ``GeckoView`` is an Android ``View`` that embedders can use to paint a
  ``GeckoSession`` in the app. Normally, only ``GeckoSession`` s associated to
  a ``GeckoView`` are actually *alive*, i.e. can receive events, fire timers,
  etc.

Delegates
---------

Because GeckoView has no UI elements and doesn't store a lot of data, it needs
a way to *delegate* behavior when Web sites need functionality that requires
these features.

To do that, GeckoView exposes Java interfaces to the embedders, called
Delegates. Delegates are normally associated to either the runtime, when they
don't refer to a specific session, or a session, when they are
session-specific.

The most important delegates are:

- ``Autocomplete.StorageDelegate`` Which is used by embedders to implement
  autocomplete functionality for logins, addresses and credit cards.
- ``ContentDelegate`` Which receives events from the content Web page like
  "open a new window", "on fullscreen request", "this tab crashed" etc.
- ``HistoryDelegate`` Which receives events about new or modified history
  entries. GeckoView itself does not store history so the app is required to
  listen to history events and store them permanently.
- ``NavigationDelegate`` Informs the embedder about navigation events and
  requests.
- ``PermissionDelegate`` Used to prompt the user for permissions like
  geolocation, notifications, etc.
- ``PromptDelegate`` Implements content-side prompts like alert(), confirm(),
  basic HTTP auth, etc.
- ``MediaSession.Delegate`` Informs the embedder about media elements currently
  active on the page and allows the embedder to pause, resume, receive playback
  state etc.
- ``WebExtension.MessageDelegate`` Used by the embedder to exchange messages
  with built-in extensions. See also `Interacting with Web Content <../consumer/web-extensions.html>`_.


GeckoDisplay
------------

GeckoView can paint to either a ``SurfaceView`` or a ``TextureView``.

- ``SufaceView`` is what most apps will use and it's the default, it provides a
  barebone wrapper around a GL surface where GeckoView can paint on.
  SurfaceView is not part of normal Android compositing, which means that
  Android is not able to paint (partially) on top of a SurfaceView or apply
  transformations and animations to it.
- ``TextureView`` offers a surface which can be transformed and animated but
  it's slower and requires more memory because it's `triple-buffered
  <https://en.wikipedia.org/wiki/Multiple_buffering#Triple_buffering>`_
  (which is necessary to offer animations).

Most apps will use the ``GeckoView`` class to paint the web page. The
``GeckoView`` class is an Android ``View`` which takes part in the Android view
hierarchy.

Android recycles the ``GeckoView`` whenever the app is not visible, releasing
the associated ``SurfaceView`` or ``TextureView``. This triggers a few actions
on the Gecko side:

- The GL Surface is released, and Gecko is notified in
  `SyncPauseCompositor <https://searchfox.org/mozilla-central/rev/ead7da2d9c5400bc7034ff3f06a030531bd7e5b9/widget/android/nsWindow.cpp#1114>`_.
- The ``<browser>`` associated to the ``GeckoSession`` is `set to inactive <https://searchfox.org/mozilla-central/rev/ead7da2d9c5400bc7034ff3f06a030531bd7e5b9/mobile/android/geckoview/src/main/java/org/mozilla/geckoview/GeckoView.java#553>`_,
  which essentially freezes the JavaScript engine.

Apps that do not use ``GeckoView``, because e.g. they cannot use
``SurfaceView``, need to manage the active state manually and call
``GeckoSession.setActive`` whenever the session is not being painted on the
screen.

Thread safety
-------------

Apps will inevitably have to deal with the Android UI in a significant way.
Most of the Android UI toolkit operates on the UI thread, and requires
consumers to execute method calls on it. The Android UI thread runs an event
loop that can be used to schedule tasks on it from other threads.

Gecko, on the other hand, has its own main thread where a lot of the front-end
interactions happen, and many methods inside Gecko expect to be called on the
main thread.

To not overburden the App with unnecessary multi-threaded code, GeckoView will
always bridge the two "main threads" and redirect method calls as appropriate.
Most GeckoView delegate calls will thus happen on the Android UI thread and
most APIs are expected to be called on the UI thread as well.

This can sometimes create unexpected performance considerations, as illustrated
in later sections.

GeckoResult
-----------

An ubiquitous tool in the GeckoView API is ``GeckoResult``. GeckoResult is a
promise-like class that can be used by apps and by Gecko to return values
asynchronously in a thread-safe way. Internally, ``GeckoResult`` will keep
track of what thread it was created on, and will execute callbacks on the same
thread using the thread's ``Handler``.

When used in Gecko, ``GeckoResult`` can be converted to ``MozPromise`` using
``MozPromise::FromGeckoResult``.

Page load
---------

    |pageload-diagram|

GeckoView offers several entry points that can be used to react to the various
stages of a page load. The interactions can be tricky and surprising so we will
go over them in details in this section.

For each page load, the following delegate calls will be issued:
``onLoadRequest``, ``onPageStart``, ``onLocationChange``,
``onProgressChange``, ``onSecurityChange``, ``onSessionStateChange``,
``onCanGoBack``, ``onCanGoForward``, ``onLoadError``, ``onPageStop``.

Most of the method calls are self-explanatory and offer the App a chance to
update the UI in response to a change in the page load state. The more
interesting delegate calls will be described below.

onPageStart and onPageStop
~~~~~~~~~~~~~~~~~~~~~~~~~~~

``onPageStart`` and ``onPageStop`` are guaranteed to appear in pairs and in
order, and denote the beginning and the end of a page load. In between a start
and stop event, multiple ``onLoadRequest`` and ``onLocationChange`` call can be
executed, denoting redirects.

onLoadRequest
~~~~~~~~~~~~~

``onLoadRequest``, which is perhaps the most important, can be used by the App
to intercept page loads. The App can either *deny* the load, which will stop
the page from loading, and handle it internally, or *allow* the
load, which will load the page in Gecko. ``onLoadRequest`` is called for all
page loads, regardless of whether they were initiated by the app itself, by Web
content, or as a result of a redirect.

When the page load originates in Web content, Gecko has to synchronously
wait for the Android UI thread to schedule the call to ``onLoadRequest`` and
for the App to respond. This normally takes a negligible amount of time, but
when the Android UI thread is busy, e.g. because the App is being painted for
the first time, the delay can be substantial. This is an area of GeckoView that
we are actively trying to improve.

onLoadError
~~~~~~~~~~~

``onLoadError`` is called whenever the page does not load correctly, e.g.
because of a network error or a misconfigured HTTPS server. The App can return
a URL to a local HTML file that will be used as error page internally by Gecko.

onLocationChange
~~~~~~~~~~~~~~~~

``onLocationChange`` is called whenever Gecko commits to a navigation and the
URL can safely displayed in the URL bar.

onSessionStateChange
~~~~~~~~~~~~~~~~~~~~

``onSessionStateChange`` is called whenever any piece of the session state
changes, e.g. form content, scrolling position, zoom value, etc. Changes are
batched to avoid calling this API too frequently.

Apps can use ``onSessionStateChange`` to store the serialized state to
disk to support restoring the session at a later time.


Extensions
----------

Extensions can be installed using ``WebExtensionController::install`` and
``WebExtensionController::installBuiltIn``, which asynchronously returns a
``WebExtension`` object that can be used to set delegates for
extension-specific behavior.

The ``WebExtension`` object is immutable, and will be replaced every time a
property changes. For instance, to disable an extension, apps can use the
``disable`` method, which will return an updated version of the
``WebExtension`` object.

Internally, all ``WebExtension`` objects representing one extension share the
same delegates, which are stored in ``WebExtensionController``.

Given the extensive sprawling amount of data associated to extensions,
extension installation persists across restarts. Existing extensions can be
listed using ``WebExtensionController::list``.

In addition to ordinary WebExtension APIs, GeckoView allows ``builtIn``
extensions to communicate to the app via native messaging. Apps can register
themselves as native apps and extensions will be able to communicate to the app
using ``connectNative`` and ``sendNativeMessage``. Further information can be
found `here <../consumer/web-extensions.html>`__.

Internals
=========

The following sections describe how Gecko and GeckoView are implemented. These
parts of GeckoView are not normally exposed to embedders.

Process Model
-------------

Internally, Gecko uses a multi-process architecture, most of the chrome code
runs in the *main* process, while content code runs in *child* processes also
called *content* processes. There are additional types of specialized processes
like the *socket* process, which runs parts of the networking code, the *gpu*
process which executes GPU commands, the *extension* process which runs most
extension content code, etc.

We intentionally do not expose our process model to embedders.

To learn more about the multi-process architecture see `Fission for GeckoView
engineers <https://gist.github.com/agi/c900f3e473ff681158c0c907e34780e4>`_.

The majority of the GeckoView Java code runs on the main process, with a thin
glue layer on the child processes, mostly contained in ``GeckoThread``.

Process priority on Android
~~~~~~~~~~~~~~~~~~~~~~~~~~~

On Android, each process is assigned a given priority. When the device is
running low on memory, or when the system wants to conserve resources, e.g.
when the screen has been off for a long period of time, or the battery is low,
Android will sort all processes in reverse priority order and kill, using a
``SIGKILL`` event, enough processes until the given free memory and resource
threshold is reached.

Processes that are necessary to the function of the device get the highest
priority, followed by apps that are currently visible and focused on the
screen, then apps that are visible (but not on focus), background processes and
so on.

Processes that do not have a UI associated to it, e.g. background services,
will normally have the lowest priority, and thus will be killed most
frequently.

To increase the priority of a service, an app can ``bind`` to it. There are
three possible ``bind`` priority values

- ``BIND_IMPORTANT``: The process will be *as important* as the process binding
  to it
- default priority: The process will have lower priority than the process
  binding to it, but still higher priority than a background service
- ``BIND_WAIVE_PRIORITY``: The bind will be ignored for priority
  considerations.

It's important to note that the priority of each service is only relative to
the priority of the app binding to it. If the app is not visible, the app
itself and all services attached to it, regardless of binding, will get
background priority (i.e. the lowest possible priority).

Process management
~~~~~~~~~~~~~~~~~~

Each Gecko process corresponds to an Android ``service`` instance, which has to
be declared in GeckoView's ``AndroidManifest.xml``.

For example, this is the definition of the ``media`` process:

.. code-block::

  <service
          android:name="org.mozilla.gecko.media.MediaManager"
          android:enabled="true"
          android:exported="false"
          android:isolatedProcess="false"
          android:process=":media">

Process creation is controlled by Gecko which interfaces to Android using
``GeckoProcessManager``, which translates Gecko's priority to Android's
``bind`` values.

Because all priorities are waived when the app is in the background, it's not
infrequent that Android kills some of GeckoView's services, while still leaving
the main process alive.

It is therefore very important that Gecko is able to recover from process
disappearing at any moment at runtime.

Shutdown
--------

Android does not provide apps with a notification whenever the app is shutting
down. As explained in the section above, apps will simply be killed whenever
the system needs to reclaim resources. This means that Gecko on Android will
never shutdown cleanly, and that shutdown actions will never execute.

Window model
------------

Internally, Gecko has the concept of *window* and *tab*. Given that GeckoView
doesn't have the concept of tab (since it might be used to build something that
is *not* a browser) we hide Gecko tabs from the GeckoView API.

Each ``GeckoSession`` corresponds to a Gecko ``window`` object with exactly one
``tab`` in it. Because of this you might see ``window`` and ``session`` used
interchangeably in the code.

Internally, Gecko uses ``window`` s for other things other than
``GeckoSession``, so we have to sometime be careful about knowing which windows
belong to GeckoView and which don't. For example, the background extension page
is implemented as a ``window`` object that doesn't paint to a surface.

EventDispatcher
---------------

The GeckoView codebase is written in C++, JavaScript and Java, it runs across
processes and often deals with asynchronous and garbage-collected code with
complex lifetime dependencies. To make all of this work together, GeckoView
uses a cross-language event-driven architecture.

The main orchestrator of this event-driven architecture is ``EventDispatcher``.
Each language has an implementation of ``EventDispatcher`` that can be used to
fire events that are reachable from any language.

Each window (i.e. each session) has its own ``EventDispatcher`` instance, which
is also present on the content process. There is also a global
``EventDispatcher`` that is used to send and receive events that are not
related to a specific session.

Events can have data associated to it, which is represented as a
``GeckoBundle`` (essentially a ``String``-keyed variant map) on the Java and
C++ side, and a plain object on the JavaScript side. Data is automatically
converted back and forth by ``EventDispatcher``.

In Java, events are fired in the same thread where the listener was registered,
which allows us to ensure that events are received in a consistent order and
data is kept consistent, so that we by and large don't have to worry about
multi-threaded issues.

JNI
---

GeckoView code uses the Java Native Interface or JNI to communicate between
Java and C++ directly. Our JNI exports are generated from the Java source code
whenever the ``@WrapForJNI`` annotation is present. For non-GeckoView code, the
list of classes for which we generate imports is defined at
``widget/android/bindings``.

The lifetime of JNI objects depends on their native implementation:

- If the class implements ``mozilla::SupportsWeakPtr``, the Java object will
  store a ``WeakPtr`` to the native object and will not own the lifetime of the
  object.
- If the class implements ``AddRef`` and ``Release`` from ``nsISupports``, the
  Java object will store a ``RefPtr`` to the native object and will hold a
  strong reference until the Java object releases the object using
  ``DisposeNative``.
- If neither cases apply, the Java object will store a C++ pointer to the
  native object.

Calling Runtime delegates from native code
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Runtime delegates can be reached directly using the ``GeckoRuntime`` singleton.
A common pattern is to expose a ``@WrapForJNI`` method on ``GeckoRuntime`` that
will call the delegate, that than can be used on the native side. E.g.

.. code:: java

  @WrapForJNI
  private void featureCall() {
    ThreadUtils.runOnUiThread(() -> {
      if (mFeatureDelegate != null) {
        mFeatureDelegate.feature();
      }
    });
  }

And then, on the native side:

.. code:: cpp

  java::GeckoRuntime::LocalRef runtime = java::GeckoRuntime::GetInstance();
  if (runtime != nullptr) {
    runtime->FeatureCall();
  }

Session delegates
~~~~~~~~~~~~~~~~~

``GeckoSession`` delegates require a little more care, as there's a copy of a
delegate for each ``window``. Normally, a method on ``android::nsWindow`` is
added which allows Gecko code to call it. A reference to ``nsWindow`` can be
obtained from a ``nsIWidget`` using ``nsWindow::From``:

.. code:: cpp

  RefPtr<nsWindow> window = nsWindow::From(widget);
  window->SessionDelegateFeature();

The ``nsWindow`` implementation can then forward the call to
``GeckoViewSupport``, which is the JNI native side of ``GeckoSession.Window``.

.. code:: cpp

  void nsWindow::SessionDelegateFeature() {
    auto acc(mGeckoViewSupport.Access());
    if (!acc) {
      return;
    }
    acc->SessionDelegateFeature(aResponse);
  }

Which can in turn forward the call to the Java side using the JNI stubs.

.. code:: cpp

  auto GeckoViewSupport::SessionDelegateFeature() {
    GeckoSession::Window::LocalRef window(mGeckoViewWindow);
    if (!window) {
      return;
    }
    window->SessionDelegateFeature();
  }

And finally, the Java implementation calls the session delegate.

.. code:: java

  @WrapForJNI
  private void sessionDelegateFeature() {
    final GeckoSession session = mOwner.get();
    if (session == null) {
      return;
    }
    ThreadUtils.postToUiThread(() -> {
      final FeatureDelegate delegate = session.getFeatureDelegate();
      if (delegate == null) {
          return;
      }
      delegate.feature();
    });
  }

Prefs
-----

`Preferences </modules/libpref/index.html>` (or prefs) are used throughtout
Gecko to configure the browser, enable custom features, etc.

GeckoView does not directly expose prefs to Apps. A limited set configuration
options is exposed through ``GeckoRuntimeSettings``.

``GeckoRuntimeSettings`` can be easily mapped to a Gecko ``pref`` using
``Pref``, e.g.

.. code:: java

  /* package */ final Pref<Boolean> mPrefExample =
     new Pref<Boolean>("example.pref", false);

The value of the pref can then be read internally using ``mPrefExample.get``
and written to using ``mPrefExample.commit``.

Front-end and back-end
----------------------

    |code-layers|

Gecko and GeckoView code can be divided in five layers:

- **Java API** the outermost code layer that is publicly accessible to
  GeckoView embedders.
- **Java Front-End** All the Java code that supports the API and talks directly
  to the Android APIs and to the JavaScript and C++ front-ends.
- **JavaScript Front-End** The main interface to the Gecko back-end (or Gecko
  proper) in GeckoView is JavaScript, we use this layer to call into Gecko and
  other utilities provided by Gecko, code lives in ``mobile/android``
- **C++ Front-End** A smaller part of GeckoView is written in C++ and interacts
  with Gecko directly, most of this code is lives in ``widget/android``.
- **C++/Rust Back-End** This is often referred to as "platform", includes all
  core parts of Gecko and is usually accessed to in GeckoView from the C++
  front-end or the JavaScript front-end.

Modules and Actors
------------------

GeckoView's JavaScript Front-End is largely divided into units called modules
and actors. For each feature, each window will have an instance of a Module, a
parent-side Actor and (potentially many) content-side Actor instances. For a
detailed description of this see `here <https://gist.github.com/agi/c900f3e473ff681158c0c907e34780e4#actors>`__.

Testing infrastructure
----------------------

For a detailed description of our testing infrastructure see `GeckoView junit
Test Framework <https://gist.github.com/agi/5154509247fbe1170b2646a5b163433e>`_.

.. |api-diagram| image:: ../assets/api-diagram.png
.. |view-runtime-session| image:: ../assets/view-runtime-session.png
.. |pageload-diagram| image:: ../assets/pageload-diagram.png
.. |code-layers| image:: ../assets/code-layers.png
