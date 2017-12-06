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
     console.log(`Something happend: ${param1}`);
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

   class myapi extends ExtensionAPI {
     getAPI(context) {
       return {
         myapi: {
           onSomething: new EventManager(context, "myapi.onSomething", fire => {
             const callback = value => {
               fire.async(value);
             };
             RegisterSomeInternalCallback(callback);
             return () => {
               UnregisterInternalCallback(callback);
             };
           }).api()
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

   class myapi extends ExtensionAPI {
     getAPI(context) {
       return {
         myapi: {
           onSomething: new EventManager(context, "myapi.onSomething", (fire, minValue) => {
             const callback = value => {
               if (value >= minValue) 
                 fire.async(value);
               }
             };
             RegisterSomeInternalCallback(callback);
             return () => {
               UnregisterInternalCallback(callback);
             };
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

   class myapi extends ExtensionAPI {
     getAPI(context) {
       return {
         myapi: {
           onSomething: new EventManager(context, "myapi.onSomething", fire => {
             const callback = async (value) => {
               let rv = await fire.async(value);
               log(`The onSomething listener returned the string ${rv}`);
             };
             RegisterSomeInternalCallback(callback);
             return () => {
               UnregisterInternalCallback(callback);
             };
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

   class myapi extends ExtensionAPI {
     getAPI(context) {
       return {
         myapi: {
           onSomething: new EventManager(context, "myapi.onSomething", fire => {
             const listener = (value) => {
               fire.async(value);
             };

             let parentEvent = context.childManager.getParentEvent("myapi.onSomething");
             parent.addListener(listener);
             return () => {
               parent.removeListener(listener);
             };
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


