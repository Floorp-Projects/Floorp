Background
==========

WebExtensions run in a sandboxed environment much like regular web content.
The purpose of extensions is to enhance the browser in a way that
regular content cannot -- WebExtensions APIs bridge this gap by exposing
browser features to extensions in a way preserves safety, reliability,
and performance.
The implementation of a WebExtension API runs with
`chrome privileges <https://developer.mozilla.org/en-US/docs/Security/Firefox_Security_Basics_For_Developers>`_.
Browser internals are accessed using
`XPCOM <https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XPCOM>`_
or `ChromeOnly WebIDL features <https://developer.mozilla.org/en-US/docs/Mozilla/WebIDL_bindings#ChromeOnly>`_.

The rest of this documentation covers how API implementations interact
with the implementation of WebExtensions.
To expose some browser feature to WebExtensions, the first step is
to design the API.  Some high-level principles for API design
are documented on the Mozilla wiki:

- `Vision for WebExtensions <https://wiki.mozilla.org/WebExtensions/Vision>`_
- `API Policies <https://wiki.mozilla.org/WebExtensions/policy>`_
- `Process for creating new APIs <https://wiki.mozilla.org/WebExtensions/NewAPIs>`_

Javascript APIs
---------------
Many WebExtension APIs are accessed directly from extensions through
Javascript.  Functions are the most common type of object to expose,
though some extensions expose properties of primitive Javascript types
(e.g., constants).
Regardless of the exact method by which something is exposed,
there are a few important considerations when designing part of an API
that is accessible from Javascript:

- **Namespace**:
  Everything provided to extensions is exposed as part of a global object
  called ``browser``.  For compatibility with Google Chrome, many of these
  features are also exposed on a global object called ``chrome``.
  Functions and other objects are not exposed directly as properties on
  ``browser``, they are organized into *namespaces*, which appear as
  properties on ``browser``.  For example, the
  `tabs API <https://developer.mozilla.org/en-US/Add-ons/WebExtensions/API/tabs>`_
  uses a namespace called ``tabs``, so all its functions and other
  properties appear on the object ``browser.tabs``.
  For a new API that provides features via Javascript, the usual practice
  is to create a new namespace with a concise but descriptive name.

- **Environments**:
  There are several different types of Javascript environments in which
  extension code can execute: extension pages, content scripts, proxy
  scripts, and devtools pages.
  Extension pages include the background page, popups, and content pages
  accessed via |getURL|_.
  When creating a new Javascript feature the designer must choose
  in which of these environments the feature will be available.
  Most Javascript features are available in extension pages,
  other environments have limited sets of API features available.

.. |getURL| replace:: ``browser.runtime.getURL()``
.. _getURL: https://developer.mozilla.org/en-US/Add-ons/WebExtensions/API/runtime/getURL

- **Permissions**:
  Many Javascript features are only present for extensions that
  include an appropriate permission in the manifest.
  The guidelines for when an API feature requires a permission are
  described in (*citation needed*).

The specific types of features that can be exposed via Javascript are:

- **Functions**:
  A function callable from Javascript is perhaps the most commonly
  used feature in WebExtension APIs.
  New API functions are asynchronous, returning a
  `Promise <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise>`_.  Even functions that do not return a result
  use Promises so that errors can be indicated asynchronously
  via a rejected Promise as opposed to a synchronously thrown Error.
  This is due to the fact that extensions run in a child process and
  many API functions require communication with the main process.
  If an API function that needs to communicate in this way returned a
  synchronous result, then all Javascript execution in the child
  process would need to be paused until a response from the main process
  was received.  Even if a function could be implemented synchronously
  within a child process, the standard practice is to make it
  asynchronous so as not to constrain the implementation of the underlying
  browser feature and make it impossible to move functionality out of the
  child process.
  Another consequence of functions using inter-process communication is
  that the parameters to a function and its return value must all be
  simple data types that can be sent between processes using the
  `structured clone algorithm <https://developer.mozilla.org/en-US/docs/Web/API/Web_Workers_API/Structured_clone_algorithm>`_.

- **Events**:
  Events complement functions (which allow an extension to call into
  an API) by allowing an event within the browser to invoke a callback
  in the extension.
  Any time an API requires an extension to pass a callback function that
  gets invoked some arbitrary number of times, that API method should be
  defined as an event.

Manifest Keys
-------------
In addition to providing functionality via Javascript, WebExtension APIs
can also take actions based on the contents of particular properties
in an extension's manifest (or even just the presence of a particular
property).
Manifest entries are used for features in which an extension specifies
some static information that is used when an extension is installed or
when it starts up (i.e., before it has the chance to run any code to use
a Javascript API).
An API may handle a manifest key and implement Javscript functionality,
see the
`browser action <https://developer.mozilla.org/en-US/Add-ons/WebExtensions/API/browserAction>`_
API for an example.

Other Considerations
--------------------
In addition to the guidelines outlined above,
there are some other considerations when designing and implementing
a WebExtension API:

- **Cleanup**: A badly written WebExtension should not be able to permanently
  leak any resources.  In particular, any action from an extension that
  causes a resource to be allocated within the browser should be
  automatically cleaned up when the extension is disabled or uninstalled.
  This is described in more detail in the section on :ref:`lifecycle`.

- **Performance**: A new WebExtension API should not add any new overhead
  to the browser when the API is not used.  That is, the implementation
  of the API should not be loaded at all unless it is actively used by
  an extension.  In addition, initialization should be delayed when
  possible -- extensions ared started relatively early in the browser
  startup process so any unnecessary work done during extension startup
  contributes directly to sluggish browser startup.

