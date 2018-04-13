Implementing a function
=======================
Implementing an API function requires at least two different pieces:
a definition for the function in the schema, and Javascript code that
actually implements the function.

Declaring a function in the API schema
--------------------------------------
An API schema definition for a simple function looks like this:

.. code-block:: json

   [
     {
       "namespace": "myapi",
       "functions": [
         {
           "name": "add",
           "type": "function",
           "description": "Adds two numbers together.",
           "async": true,
           "parameters": [
             {
               "name": "x",
               "type": "number",
               "description": "The first number to add."
             },
             {
               "name": "y",
               "type": "number",
               "description": "The second number to add."
             }
           ]
         }
       ]
     }
   ]

The ``type`` and ``description`` properties were described above.
The ``name`` property is the name of the function as it appears in
the given namespace.  That is, the fragment above creates a function
callable from an extension as ``browser.myapi.add()``.
The ``parameters`` property describes the parameters the function takes.
Parameters are specified as an array of Javascript types, where each
parameter is a constrained Javascript value as described
in the previous section.

Each parameter may also contain additional properties ``optional``
and ``default``.  If ``optional`` is present it must be a boolean
(and parameters are not optional by default so this property is typically
only added when it has the value ``true``).
The ``default`` property is only meaningful for optional parameters,
it specifies the value that should be used for an optional parameter
if the function is called without that parameter.
An optional parameter without an explicit ``default`` property will
receive a default value of ``null``.
Although it is legal to create optional parameters at any position
(i.e., optional parameters can come before required parameters), doing so
leads to difficult to use functions and API designers are encouraged to
use object-valued parameters with optional named properties instead,
or if optional parameters must be used, to use them sparingly and put
them at the end of the parameter list.

.. XXX should we describe allowAmbiguousArguments?

The boolean-valued ``async`` property specifies whether a function
is asynchronous.
For asynchronous functions,
the WebExtensions framework takes care of automatically generating a
`Promise <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise>`_ and then resolving the Promise when the function
implementation completes (or rejecting the Promise if the implementation
throws an Error).
Since extensions can run in a child process, any API function that is
implemented (either partially or completely) in the parent process must
be asynchronous.

When a function is declared in the API schema, a wrapper for the function
is automatically created and injected into appropriate extension Javascript
contexts.  This wrapper automatically validates arguments passed to the
function against the formal parameters declared in the schema and immediately
throws an Error if invalid arguments are passed.
It also processes optional arguments and inserts default values as needed.
As a result, API implementations generally do not need to write much
boilerplate code to validate and interpret arguments.

Implementing a function in the main process
-------------------------------------------
If an asynchronous function is not implemented in the child process,
the wrapper generated from the schema automatically marshalls the
function arguments, sends the request to the parent process,
and calls the implementation there.
When that function completes, the return value is sent back to the child process
and the Promise for the function call is resolved with that value.

Based on this, an implementation of the function we wrote the schema
for above looks like this:

.. code-block:: js

   this.myapi = class extends ExtensionAPI {
     getAPI(context) {
       return {
         myapi: {
           add(x, y) { return x+y; }
         }
       }
     }
   }

The implementations of API functions are contained in a subclass of the
`ExtensionAPI <reference.html#extensionapi-class>`_ class.
Each subclass of ExtensionAPI must implement the ``getAPI()`` method
which returns an object with a structure that mirrors the structure of
functions and events that the API exposes.
The ``context`` object passed to ``getAPI()`` is an instance of
`BaseContext <reference.html#basecontext-class>`_,
which contains a number of useful properties and methods.

If an API function implementation returns a Promise, its result will
be sent back to the child process when the Promise is settled.
Any other return type will be sent directly back to the child process.
A function implementation may also raise an Error.  But by default,
an Error thrown from inside an API implementation function is not
exposed to the extension code that called the function -- it is
converted into generic errors with the message "An unexpected error occurred".
To throw a specific error to extensions, use the ``ExtensionError`` class:

.. code-block:: js

   this.myapi = class extends ExtensionAPI {
     getAPI(context) {
       return {
         myapi: {
           doSomething() {
             if (cantDoSomething) {
               throw new ExtensionError("Cannot call doSomething at this time");
             }
             return something();
           }
         }
       }
     }
   }

The purpose of this step is to avoid bugs in API implementations from
exposing details about the implementation to extensions.  When an Error
that is not an instance of ExtensionError is thrown, the original error
is logged to the
`Browser Console <https://developer.mozilla.org/en-US/docs/Tools/Browser_Console>`_,
which can be useful while developing a new API.

Implementing a function in a child process
------------------------------------------
Most functions are implemented in the main process, but there are
occasionally reasons to implement a function in a child process, such as:

- The function has one or more parameters of a type that cannot be automatically
  sent to the main process using the structured clone algorithm.

- The function implementation interacts with some part of the browser
  internals that is only accessible from a child process.

- The function can be implemented substantially more efficiently in
  a child process.

To implement a function in a child process, simply include an ExtensionAPI
subclass that is loaded in the appropriate context
(e.g, ``addon_child``, ``content_child``, etc.) as described in
the section on :ref:`basics`.
Code inside an ExtensionAPI subclass in a child process may call the
implementation of a function in the parent process using a method from
the API context as follows:

.. code-block:: js

   this.myapi = class extends ExtensionAPI {
     getAPI(context) {
       return {
         myapi: {
           async doSomething(arg) {
             let result = await context.childManager.callParentAsyncFunction("anothernamespace.functionname", [arg]);
             /* do something with result */
             return ...;
           }
         }
       }
     }
   }

As you might expect, ``callParentAsyncFunction()`` calls the given function
in the main process with the given arguments, and returns a Promise
that resolves with the result of the function.
This is the same mechanism that is used by the automatically generated
function wrappers for asynchronous functions that do not have a
provided implementation in a child process.

It is possible to define the same function in both the main process
and a child process and have the implementation in the child process
call the function with the same name in the parent process.
This is a common pattern when the implementation of a particular function
requires some code in both the main process and child process.
