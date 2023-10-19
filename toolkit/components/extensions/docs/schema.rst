API Schemas
===========
Anything that a WebExtension API exposes to extensions via Javascript
is described by the API's schema.  The format of API schemas uses some
of the same syntax as `JSON Schema <http://json-schema.org/>`_.
JSON Schema provides a way to specify constraints on JSON documents and
the same method is used by WebExtensions to specify constraints on,
for example, parameters passed to an API function.  But the syntax for
describing functions, namespaces, etc. is all ad hoc.  This section
describes that syntax.

An individual API schema consists of structured descriptions of
items in one or more *namespaces* using a structure like this:

.. code-block:: js

   [
     {
       "namespace": "namespace1",
       // declarations for namespace 1...
     },
     {
       "namespace": "namespace2",
       // declarations for namespace 2...
     },
     // other namespaces...
   ]

Most of the namespaces correspond to objects available to extensions
Javascript code under the ``browser`` global.  For example, entries in the
namespace ``example`` are accessible to extension Javascript code as
properties on ``browser.example``.
The namespace ``"manifest"`` is handled specially, it describes the
structure of WebExtension manifests (i.e., ``manifest.json`` files).
Manifest schemas are explained in detail below.

Declarations within a namespace look like:

.. code-block:: js

   {
     "namespace": "namespace1",
     "types": [
       { /* type definition */ },
       ...
     ],
     "properties": {
       "NAME": { /* property definition */ },
       ...
     },
     "functions": [
       { /* function definition */ },
       ...
     ],
     "events": [
       { /* event definition */ },
       ...
     ]
   }

The four types of objects that can be defined inside a namespace are:

- **types**: A type is a reusable schema fragment.  A common use of types
  is to define in one place an object with a particular set of typed fields
  that is used in multiple places in an API.

- **properties**: A property is a fixed Javascript value available to
  extensions via Javascript.  Note that the format for defining
  properties in a schema is different from the format for types, functions,
  and events.  The next subsection describes creating properties in detail.

- **functions** and **events**:
  These entries create functions and events respectively, which are
  usable from Javascript by extensions.  Details on how to implement
  them are later in this section.

Implementing a fixed Javascript property
----------------------------------------
A static property is made available to extensions via Javascript
entirely from the schema, using a fragment like this one:

.. code-block:: js

   [
     "namespace": "myapi",
     "properties": {
       "SOME_PROPERTY": {
        "value": 24,
        "description": "Description of my property here."
       }
     }
   ]

If a WebExtension API with this fragment in its schema is loaded for
a particular extension context, that extension will be able to access
``browser.myapi.SOME_PROPERTY`` and read the fixed value 24.
The contents of ``value`` can be any JSON serializable object.

Schema Items
------------
Most definitions of individual items in a schema have a common format:

.. code-block:: js

   {
     "type": "SOME TYPE",
     /* type-specific parameters... */
   }

Type-specific parameters will be described in subsequent sections,
but there are some optional properties that can appear in many
different types of items in an API schema:

- ``description``: This string-valued property serves as documentation
  for anybody reading or editing the schema.

- ``permissions``: This property is an array of strings.
  If present, the item in which this property appears is only made
  available to extensions that have all the permissions listed in the array.

- ``unsupported``: This property must be a boolean.
  If it is true, the item in which it appears is ignored.
  By using this property, a schema can define how a particular API
  is intended to work, before it is implemented.

- ``deprecated``: This property must be a boolean.  If it is true,
  any uses of the item in which it appears will cause a warning to
  be logged to the browser console, to indicate to extension authors
  that they are using a feature that is deprecated or otherwise
  not fully supported.


Describing constrained values
-----------------------------
There are many places where API schemas specify constraints on the type
and possibly contents of some JSON value (e.g., the manifest property
``name`` must be a string) or Javascript value (e.g., the first argument
to ``browser.tabs.get()`` must be a non-negative integer).
These items are defined using `JSON Schema <http://json-schema.org/>`_.
Specifically, these items are specified by using one of the following
values for the ``type`` property: ``boolean``, ``integer``, ``number``,
``string``, ``array``, ``object``, or ``any``.
Refer to the documentation and examples at the
`JSON Schema site <http://json-schema.org/>`_ for details on how these
items are defined in a schema.
