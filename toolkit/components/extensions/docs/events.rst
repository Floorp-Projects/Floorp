Implementing an event
=====================
Like a function, an event requires a definition in the schema and
an implementation in Javascript inside an instance of ExtensionAPI.

Declaring an event in the API schema
------------------------------------
The definition for a simple event looks like this:

.. code-block:: json

   [
     {
       "namespace": "myapi",
       "events": [
         {
           "name": "onSomething",
           "type": "function",
           "description": "Description of the event",
           "parameters": [
             {
               "name": "param1",
               "description": "Description of the first callback parameter",
               "type": "number"
             }
           ]
         }
       ]
     }
   ]

This fragment defines an event that is used from an extension with
code such as:

.. code-block:: js

   browser.myapi.onSomething.addListener(param1 => {
     console.log(`Something happened: ${param1}`);
   });

Note that the schema syntax looks similar to that for a function,
but for an event, the ``parameters`` property specifies the arguments
that will be passed to a listener.

Implementing an event
---------------------
Just like with functions, defining an event in the schema causes
wrappers to be automatically created and exposed to an extensions'
appropriate Javascript contexts.
An event appears to an extension as an object with three standard
function properties: ``addListener()``, ``removeListener()``,
and ``hasListener()``.
Also like functions, if an API defines an event but does not implement
it in a child process, the wrapper in the child process effectively
proxies these calls to the implementation in the main process.

A helper class called
`EventManager <reference.html#eventmanager-class>`_ makes implementing
events relatively simple.  A simple event implementation looks like:

.. code-block:: js

   this.myapi = class extends ExtensionAPI {
     getAPI(context) {
       return {
         myapi: {
           onSomething: new EventManager({
             context,
             name: "myapi.onSomething",
             register: fire => {
               const callback = value => {
                 fire.async(value);
               };
               RegisterSomeInternalCallback(callback);
               return () => {
                 UnregisterInternalCallback(callback);
               };
             }
           }).api(),
         }
       }
     }
   }

The ``EventManager`` class is usually just used directly as in this example.
The first argument to the constructor is an ``ExtensionContext`` instance,
typically just the object passed to the API's ``getAPI()`` function.
The second argument is a name, it is used only for debugging.
The third argument is the important piece, it is a function that is called
the first time a listener is added for this event.
This function is passed an object (``fire`` in the example) that is used to
invoke the extension's listener whenever the event occurs.  The ``fire``
object has several different methods for invoking listeners, but for
events implemented in the main process, the only valid method is
``async()`` which executes the listener asynchronously.

The event setup function (the function passed to the ``EventManager``
constructor) must return a cleanup function,
which will be called when the listener is removed either explicitly
by the extension by calling ``removeListener()`` or implicitly when
the extension Javascript context from which the listener was added is destroyed.

In this example, ``RegisterSomeInternalCallback()`` and
``UnregisterInternalCallback()`` represent methods for listening for
some internal browser event from chrome privileged code.  This is
typically something like adding an observer using ``Services.obs`` or
attaching a listener to an ``EventEmitter``.

After constructing an instance of ``EventManager``, its ``api()`` method
returns an object with with ``addListener()``, ``removeListener()``, and
``hasListener()`` methods.  This is the standard extension event interface,
this object is suitable for returning from the extension's
``getAPI()`` method as in the example above.

Handling extra arguments to addListener()
-----------------------------------------
The standard ``addListener()`` method for events may accept optional
addition parameters to allow extra information to be passed when registering
an event listener.  One common application of this parameter is for filtering,
so that extensions that only care about a small subset of the instances of
some event can avoid the overhead of receiving the ones they don't care about.

Extra parameters to ``addListener()`` are defined in the schema with the
the ``extraParameters`` property.  For example:

.. code-block:: json

   [
     {
       "namespace": "myapi",
       "events": [
         {
           "name": "onSomething",
           "type": "function",
           "description": "Description of the event",
           "parameters": [
             {
               "name": "param1",
               "description": "Description of the first callback parameter",
               "type": "number"
             }
           ],
           "extraParameters": [
             {
               "name": "minValue",
               "description": "Only call the listener for values of param1 at least as large as this value.",
               "type": "number"
             }
           ]
         }
       ]
     }
   ]

Extra parameters defined in this way are passed to the event setup
function (the last parameter to the ``EventManager`` constructor.
For example, extending our example above:

.. code-block:: js

   this.myapi = class extends ExtensionAPI {
     getAPI(context) {
       return {
         myapi: {
           onSomething: new EventManager({
             context,
             module: "myapi",
             event: "onSomething",
             register: (fire, minValue) => {
               const callback = value => {
                 if (value >= minValue) {
                   fire.async(value);
                 }
               };
               RegisterSomeInternalCallback(callback);
               return () => {
                 UnregisterInternalCallback(callback);
               };
             }
           }).api()
         }
       }
     }
   }

Handling listener return values
-------------------------------
Some event APIs allow extensions to affect event handling in some way
by returning values from event listeners that are processed by the API.
This can be defined in the schema with the ``returns`` property:

.. code-block:: json

   [
     {
       "namespace": "myapi",
       "events": [
         {
           "name": "onSomething",
           "type": "function",
           "description": "Description of the event",
           "parameters": [
             {
               "name": "param1",
               "description": "Description of the first callback parameter",
               "type": "number"
             }
           ],
           "returns": {
             "type": "string",
             "description": "Description of how the listener return value is processed."
           }
         }
       ]
     }
   ]

And the implementation of the event uses the return value from ``fire.async()``
which is a Promise that resolves to the listener's return value:

.. code-block:: js

   this.myapi = class extends ExtensionAPI {
     getAPI(context) {
       return {
         myapi: {
           onSomething: new EventManager({
             context,
             module: "myapi",
             event: "onSomething",
             register: fire => {
               const callback = async (value) => {
                 let rv = await fire.async(value);
                 log(`The onSomething listener returned the string ${rv}`);
               };
               RegisterSomeInternalCallback(callback);
               return () => {
                 UnregisterInternalCallback(callback);
               };
             }
           }).api()
         }
       }
     }
   }

Note that the schema ``returns`` definition is optional and serves only
for documentation.  That is, ``fire.async()`` always returns a Promise
that resolves to the listener return value, the implementation of an
event can just ignore this Promise if it doesn't care about the return value.

Implementing an event in the child process
------------------------------------------
The reasons for implementing events in the child process are similar to
the reasons for implementing functions in the child process:

- Listeners for the event return a value that the API implementation must
  act on synchronously.

- Either ``addListener()`` or the listener function has one or more
  parameters of a type that cannot be sent between processes.

- The implementation of the event interacts with code that is only
  accessible from a child process.

- The event can be implemented substantially more efficiently in a
  child process.

The process for implementing an event in the child process is the same
as for functions -- simply implement the event in an ExtensionAPI subclass
that is loaded in a child process.  And just as a function in a child
process can call a function in the main process with
`callParentAsyncFunction()`, events in a child process may subscribe to
events implemented in the main process with a similar `getParentEvent()`.
For example, the automatically generated event proxy in a child process
could be written explicitly as:

.. code-block:: js

   this.myapi = class extends ExtensionAPI {
     getAPI(context) {
       return {
         myapi: {
           onSomething: new EventManager(
             context,
             name: "myapi.onSomething",
             register: fire => {
               const listener = (value) => {
                 fire.async(value);
               };

               let parentEvent = context.childManager.getParentEvent("myapi.onSomething");
               parent.addListener(listener);
               return () => {
                 parent.removeListener(listener);
               };
             }
           }).api()
         }
       }
     }
   }

Events implemented in a child process have some additional methods available
to dispatch listeners:

- ``fire.sync()`` This runs the listener synchronously and returns the
  value returned by the listener

- ``fire.raw()`` This runs the listener synchronously without cloning
  the listener arguments into the extension's Javascript compartment.
  This is used as a performance optimization, it should not be used
  unless you have a detailed understanding of Javascript compartments
  and cross-compartment wrappers.

Event Listeners Persistence
---------------------------

Event listeners are persisted in some circumstances.  Persisted event listeners can either
block startup, and/or cause an Event Page or Background Service Worker to be started.

The event listener must be registered synchronously in the top level scope
of the background.  Event listeners registered later, or asynchronously, are
not persisted.

Currently only WebRequestBlocking and Proxy events are able to block
at startup, causing an addon to start earlier in Firefox startup.  Whether
a module can block startup is defined by a ``startupBlocking`` flag in
the module definition files (``ext-toolkit.json`` or ``ext-browser.json``).
As well, these are the only events persisted for persistent background scripts.

Events implemented only in a child process, without a parent process counterpart,
cannot be persisted.

Persisted and Primed Event Listeners
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In terms of terminology:

- **Persisted Event Listener** is the set of data (in particular API module, API event name
  and the parameters passed along with addListener call if any) related to an event listener
  that has been registered by an Event Page (or Background Service Worker) in a previous run
  and being stored in the StartupCache data

- **Primed Event Listener** is a "placeholder" event listener created, from the **Persisted Event Listener**
  data found in the StartupCache, while the Event Page (or Background Service Worker) is not running
  (either not started yet or suspended after the idle timeout was hit)

ExtensionAPIPersistent and PERSISTENT_EVENTS
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Most of the WebExtensions APIs promise some API events, and it is likely that most of those events are also
expected to be waking up the Event Page (or Background Service Worker) when emitted while the background
extension context has not been started yet (or it was suspended after the idle timeout was hit).

As part of implementing a WebExtensions API that is meant to persist all or some of its API event
listeners:

- the WebExtensions API namespace class should extend ``ExtensionAPIPersistent`` (instead of extending
  the ``ExtensionAPI`` class)

- the WebExtensions API namespace should have a ``PERSISTENT_EVENTS`` property, which is expected to be
  set to an object defining methods. Each method should be named after the related API event name, which
  are going to be called internally:

  - while the extension Event Page (or Background Service Worker) isn't running (either never started yet
    or suspended after the idle timeout). These methods are called by the WebExtensions internals to
    create placeholder API event listeners in the parent process for each of the API event listeners
    persisted for that extension. These placeholder listeners are internally referred to as
    ``primed listeners``).

  - while the extension Event Page (or Background Service Worker) is running (as well as for any other
    extension context types they may have been created for the extension). These methods are called by the
    WebExtensions internals to create the parent process callback that will be responsible for
    forwarding the API events to the extension callbacks in the child processes.

- in the ``getAPI`` method. For all the API namespace properties that represent API events returned by this method,
  the ``EventManager`` instances created for each of the API events that is expected to persist its listeners
  should include following options:

  - ``module``, to be set to the API module name as listed in ``"ext-toolkit.json"`` / ``"ext-browser.json"``
    / ``"ext-android.json"`` (which, in most cases, is the same as the API namespace name string).
  - ``event``, to be set to the API event name string.
  - ``extensionApi``, to be set to the ``ExtensionAPIPersistent`` class instance.

Taking a look to some of the patches landed to introduce API event listener persistency on some of the existing
API as part of introducing support for the Event Page may also be useful:

- Bug-1748546_ ported the browserAction and pageAction API namespace implementations to
  ``ExtensionAPIPersistent`` and, in particular, the changes applied to:

  - ext-browserAction.js: https://hg.mozilla.org/integration/autoland/rev/08a3eaa8bce7
  - ext-pageAction.js: https://hg.mozilla.org/integration/autoland/rev/ed616e2e0abb

.. _Bug-1748546: https://bugzilla.mozilla.org/show_bug.cgi?id=1748546

Follows an example of what has been described previously in a code snippet form:

.. code-block:: js

   this.myApiName = class extends ExtensionAPIPersistent {
     PERSISTENT_EVENTS = {
       // @param {object}             options
       // @param {object}             options.fire
       // @param {function}           options.fire.async
       // @param {function}           options.fire.sync
       // @param {function}           options.fire.raw
       //        For primed listeners `fire.async`/`fire.sync`/`fire.raw` will
       //        collect the pending events to be send to the background context
       //        and implicitly wake up the background context (Event Page or
       //        Background Service Worker), or forward the event right away if
       //        the background context is running.
       // @param {function}           [options.fire.wakeup = undefined]
       //        For primed listeners, the `fire` object also provide a `wakeup` method
       //        which can be used by the primed listener to explicitly `wakeup` the
       //        background context (Event Page or Background Service Worker) and wait for
       //        it to be running (by awaiting on the Promise returned by wakeup to be
       //        resolved).
       // @param {ProxyContextParent} [options.context=undefined]
       //        This property is expected to be undefined for primed listeners (which
       //        are created while the background extension context does not exist) and
       //        to be set to a ProxyContextParent instance (the same got by the getAPI
       //        method) when the method is called for a listener registered by a
       //        running extension context.
       //
       // @param {object}            [apiEventsParams=undefined]
       //        The additional addListener parameter if any (some API events are allowing
       //        the extensions to pass some parameters along with the extension callback).
       onMyEventName({ context, fire }, apiEventParams = undefined) {
         const listener = (...) {
           // Wake up the EventPage (or Background ServiceWorker).
           if (fire.wakeup) {
             await fire.wakeup();
           }

           fire.async(...);
         }

         // Subscribe a listener to an internal observer or event which will be notified
         // when we need to call fire to either send the event to an extension context
         // already running or wake up a suspended event page and accumulate the events
         // to be fired once the extension context is running again and a callback registered
         // back (which will be used to convert the primed listener created while
         // the non persistent background extension context was not running yet)
         ...
         return {
           unregister() {
             // Unsubscribe a listener from an internal observer or event.
             ...
            }
           convert(fireToExtensionCallback) {
             // Convert gets called once the primed API event listener,
             // created while the extension background context has been
             // suspended, is being converted to a parent process API
             // event listener callback that is responsible for forwarding the
             // events to the child processes.
             //
             // The `fireToExtensionCallback` parameter is going to be the
             // one that will emit the event to the extension callback (while
             // the one got from the API event registrar method may be the one
             // that is collecting the events to emit up until the background
             // context got started up again).
             fire = fireToExtensionCallback;
           },
         };
       },
       ...
     };

     getAPI(context) {
       ...
       return {
         myAPIName: {
           ...
           onMyEventName: new EventManager({
             context,
             // NOTE: module is expected to be the API module name as listed in
             // ext-toolkit.json / ext-browser.json / ext-android.json.
             module: "myAPIName",
             event: "onMyEventNAme",
             extensionApi: this,
           }),
         },
       };
     }
   };

Testing Persisted API Event Listeners
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- ``extension.terminateBackground()`` / ``extension.terminateBackground({ disableResetIdleForTest: true})``:

  - The wrapper object returned by ``ExtensionTestUtils.loadExtension`` provides
    a ``terminateBackground`` method which can be used to simulate an idle timeout,
    by explicitly triggering the same idle timeout suspend logic handling the idle timeout.
  - This method also accept an optional parameter, if set to ``{ disableResetIdleForTest: true}``
    will forcefully suspend the background extension context and ignore all the
    conditions that would reset the idle timeout due to some work still pending
    (e.g. a ``NativeMessaging``'s ``Port`` still open, a ``StreamFilter`` instance
    still active or a ``Promise`` from an API event listener call not yet resolved)

- ``ExtensionTestUtils.testAssertions.assertPersistentListeners``:

  - This test assertion helper can be used to more easily assert what should
    be the persisted state of a given API event (e.g. assert it to not be
    persisted, or to be persisted and/or primed)

.. code-block:: js

   assertPersistentListeners(extension, "browserAction", "onClicked", {
      primed: false,
    });
    await extension.terminateBackground();
    assertPersistentListeners(extension, "browserAction", "onClicked", {
      primed: true,
    });

- ``extensions.background.idle.timeout`` preference determines how long to wait
  (between API events being notified to the extension event page) before considering
  the Event Page in the idle state and suspend it, in some xpcshell test this pref
  may be set to 0 to reduce the amount of time the test will have to wait for the
  Event Page to be suspended automatically

- ``extension.eventPage.enabled`` pref is responsible for enabling/disabling
  Event Page support for manifest_version 2 extension, technically it is
  now set to ``true`` on all channels, but it would still be worth flipping it
  to ``true`` explicitly in tests that are meant to cover Event Page behaviors
  for manifest_version 2 test extension until the pref is completely removed
  (mainly to make sure that if the pref would need to be flipped to false
  for any reason, the tests will still be passing)

Persisted Event listeners internals
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The ``ExtensionAPIPersistent`` class provides a way to quickly introduce API event
listener persistency to a new WebExtensions API, and reduce the number of code
duplication, the following section provide some more details about what the
abstractions are doing internally in practice.

WebExtensions APIs classes that extend the ``ExtensionAPIPersistent`` base class
are still able to support non persisted listeners along with persisted ones
(e.g. events that are persisting the listeners registered from an event page are
already not persisting listeners registered from other extension contexts)
and can mix persisted and non-persisted events.

As an example in ``toolkit/components/extensions/parent/ext-runtime.js``` the two
events ``onSuspend`` and ``onSuspendCanceled`` are expected to be never persisted
nor primed (even for an event page) and so their ``EventManager`` instances
receive the following options:

- a ``register`` callback (instead of the one part of ``PERSISTED_EVENTS``)
- a ``name`` string property (instead of the two separate ``module`` and ``event``
  string properties that are used for ``EventManager`` instances from persisted
  ones
- no ``extensionApi`` property (because that is only needed for events that are
  expected to persist event page listeners)

In practice ``ExtensionAPIPersistent`` extends the ``ExtensionAPI`` class to provide
a generic ``primeListeners`` method, which is the method responsible for priming
a persisted listener when the event page has been suspended or not started yet.

The ``primeListener`` method is expected to return an object with an ``unregister``
and ``convert`` method, while the ``register`` callback passed to  the ``EventManager``
constructor is expected to return the ``unregister`` method.

.. code-block:: js

   function somethingListener(fire, minValue) => {
     const callback = value => {
       if (value >= minValue) {
         fire.async(value);
       }
     };
     RegisterSomeInternalCallback(callback);
     return {
       unregister() {
         UnregisterInternalCallback(callback);
       },
       convert(_fire, context) {
         fire = _fire;
       }
     };
   }

   this.myapi = class extends ExtensionAPI {
     primeListener(extension, event, fire, params, isInStartup) {
       if (event == "onSomething") {
         // Note that we return the object with unregister and convert here.
         return somethingListener(fire, ...params);
       }
       // If an event other than onSomething was requested, we are not returning
       // anything for it, thus it would not be persistable.
     }
     getAPI(context) {
       return {
         myapi: {
           onSomething: new EventManager({
             context,
             module: "myapi",
             event: "onSomething",
             register: (fire, minValue) => {
               // Note that we return unregister here.
               return somethingListener(fire, minValue).unregister;
             }
           }).api()
         }
       }
     }
   }
