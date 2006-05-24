2006-04-29      v1.3.1 (bug fix release)

- Fix sendXMLHttpRequest sendContent regression
- Internet Explorer fix in MochiKit.Logging (printfire exception)
- Internet Explorer XMLHttpRequest object leak fixed in MochiKit.Async

2006-04-26      v1.3 "warp zone"

- IMPORTANT: Renamed MochiKit.Base.forward to forwardCall (for export)
- IMPORTANT: Renamed MochiKit.Base.find to findValue (for export)
- New MochiKit.Base.method as a convenience form of bind that takes the
  object before the method
- New MochiKit.Base.flattenArguments for flattening a list of arguments to
  a single Array
- Refactored MochiRegExp example to use MochiKit.Signal
- New key_events example demonstrating use of MochiKit.Signal's key handling
  capabilities.
- MochiKit.DOM.createDOM API change for convenience: if attrs is a string,
  null is used and the string will be considered the first node. This
  allows for the more natural P("foo") rather than P(null, "foo").
- MochiKit Interpreter example refactored to use MochiKit.Signal and now
  provides multi-line input and a help() function to get MochiKit function
  signature from the documentation.
- Native Console Logging for the default MochiKit.Logging logger
- New MochiKit.Async.DeferredList, gatherResults, maybeDeferred
- New MochiKit.Signal example: draggable
- Added sanity checking to Deferred to ensure that errors happen when chaining
  is used incorrectly
- Opera sendXMLHttpRequest fix (sends empty string instead of null by default)
- Fix a bug in MochiKit.Color that incorrectly generated hex colors for
  component values smaller than 16/255.
- Fix a bug in MochiKit.Logging that prevented logs from being capped at a
  maximum size
- MochiKit.Async.Deferred will now wrap thrown objects that are not instanceof
  Error, so that the errback chain is used instead of the callback chain.
- MochiKit.DOM.appendChildNodes and associated functions now append iterables
  in the correct order.
- New MochiKit-based SimpleTest test runner as a replacement for Test.Simple
- MochiKit.Base.isNull no longer matches undefined
- example doctypes changed to HTML4
- isDateLike no longer throws error on null
- New MochiKit.Signal module, modeled after the slot/signal mechanism in Qt
- updated elementDimensions to calculate width from offsetWidth instead
  of clientWidth 
- formContents now works with FORM tags that have a name attribute
- Documentation now uses MochiKit to generate a function index

2006-01-26      v1.2 "the ocho"

- Fixed MochiKit.Color.Color.lighterColorWithLevel
- Added new MochiKit.Base.findIdentical function to find the index of an
  element in an Array-like object. Uses === for identity comparison.
- Added new MochiKit.Base.find function to find the index of an element in
  an Array-like object. Uses compare for rich comparison.
- MochiKit.Base.bind will accept a string for func, which will be immediately
  looked up as self[func].
- MochiKit.DOM.formContents no longer skips empty form elements for Zope
  compatibility
- MochiKit.Iter.forEach will now catch StopIteration to break
- New MochiKit.DOM.elementDimensions(element) for determining the width and
  height of an element in the document
- MochiKit.DOM's initialization is now compatible with
  HTMLUnit + JWebUnit + Rhino
- MochiKit.LoggingPane will now re-use a ``_MochiKit_LoggingPane`` DIV element
  currently in the document instead of always creating one.
- MochiKit.Base now has operator.mul
- MochiKit.DOM.formContents correctly handles unchecked checkboxes that have
  a custom value attribute
- Added new MochiKit.Color constructors fromComputedStyle and fromText
- MochiKit.DOM.setNodeAttribute should work now
- MochiKit.DOM now has a workaround for an IE bug when setting the style
  property to a string
- MochiKit.DOM.createDOM now has workarounds for IE bugs when setting the
  name and for properties
- MochiKit.DOM.scrapeText now walks the DOM tree in-order
- MochiKit.LoggingPane now sanitizes the window name to work around IE bug
- MochiKit.DOM now translates usemap to useMap to work around IE bug
- MochiKit.Logging is now resistant to Prototype's dumb Object.prototype hacks
- Added new MochiKit.DOM documentation on element visibility
- New MochiKit.DOM.elementPosition(element[, relativeTo={x: 0, y: 0}])
  for determining the position of an element in the document
- Added new MochiKit.DOM createDOMFunc aliases: CANVAS, STRONG

2005-11-14      v1.1

- Fixed a bug in numberFormatter with large numbers
- Massively overhauled documentation
- Fast-path for primitives in MochiKit.Base.compare
- New groupby and groupby_as_array in MochiKit.Iter
- Added iterator factory adapter for objects that implement iterateNext()
- Fixed isoTimestamp to handle timestamps with time zone correctly
- Added new MochiKit.DOM createDOMFunc aliases: SELECT, OPTION, OPTGROUP, 
  LEGEND, FIELDSET
- New MochiKit.DOM formContents and enhancement to queryString to support it
- Updated view_source example to use dp.SyntaxHighlighter 1.3.0
- MochiKit.LoggingPane now uses named windows based on the URL so that
  a given URL will get the same LoggingPane window after a reload
  (at the same position, etc.)
- MochiKit.DOM now has currentWindow() and currentDocument() context
  variables that are set with withWindow() and withDocument(). These
  context variables affect all MochiKit.DOM functionality (getElement,
  createDOM, etc.)
- MochiKit.Base.items will now catch and ignore exceptions for properties
  that are enumerable but not accessible (e.g. permission denied)
- MochiKit.Async.Deferred's addCallback/addErrback/addBoth
  now accept additional arguments that are used to create a partially
  applied function. This differs from Twisted in that the callback/errback
  result becomes the *last* argument, not the first when this feature
  is used.
- MochiKit.Async's doSimpleXMLHttpRequest will now accept additional
  arguments which are used to create a GET query string
- Did some refactoring to reduce the footprint of MochiKit by a few
  kilobytes
- escapeHTML to longer escapes ' (apos) and now uses
  String.replace instead of iterating over every char.
- Added DeferredLock to Async
- Renamed getElementsComputedStyle to computedStyle and moved
  it from MochiKit.Visual to MochiKit.DOM
- Moved all color support out of MochiKit.Visual and into MochiKit.Color
- Fixed range() to accept a negative step
- New alias to MochiKit.swapDOM called removeElement
- New MochiKit.DOM.setNodeAttribute(node, attr, value) which sets
  an attribute on a node without raising, roughly equivalent to:
  updateNodeAttributes(node, {attr: value})
- New MochiKit.DOM.getNodeAttribute(node, attr) which gets the value of
  a node's attribute or returns null without raising
- Fixed a potential IE memory leak if using MochiKit.DOM.addToCallStack
  directly (addLoadEvent did not leak, since it clears the handler)

2005-10-24      v1.0

- New interpreter example that shows usage of MochiKit.DOM  to make
  an interactive JavaScript interpreter
- New MochiKit.LoggingPane for use with the MochiKit.Logging
  debuggingBookmarklet, with logging_pane example to show its usage
- New mochiregexp example that demonstrates MochiKit.DOM and MochiKit.Async
  in order to provide a live regular expression matching tool
- Added advanced number formatting capabilities to MochiKit.Format:
  numberFormatter(pattern, placeholder="", locale="default") and
  formatLocale(locale="default")
- Added updatetree(self, obj[, ...]) to MochiKit.Base, and changed
  MochiKit.DOM's updateNodeAttributes(node, attrs) to use it when appropiate.
- Added new MochiKit.DOM createDOMFunc aliases: BUTTON, TT, PRE
- Added truncToFixed(aNumber, precision) and roundToFixed(aNumber, precision)
  to MochiKit.Format
- MochiKit.DateTime can now handle full ISO 8601 timestamps, specifically
  isoTimestamp(isoString) will convert them to Date objects, and
  toISOTimestamp(date, true) will return an ISO 8601 timestamp in UTC
- Fixed missing errback for sendXMLHttpRequest when the server does not
  respond
- Fixed infinite recusion bug when using roundClass("DIV", ...)
- Fixed a bug in MochiKit.Async wait (and callLater) that prevented them
  from being cancelled properly
- Workaround in MochiKit.Base bind (and partial) for functions that don't
  have an apply method, such as alert
- Reliably return null from the string parsing/manipulation functions if
  the input can't be coerced to a string (s + "") or the input makes no sense;
  e.g. isoTimestamp(null) and isoTimestamp("") return null

2005-10-08      v0.90

- Fixed ISO compliance with toISODate
- Added missing operator.sub
- Placated Mozilla's strict warnings a bit
- Added JSON serialization and unserialization support to MochiKit.Base:
  serializeJSON, evalJSON, registerJSON. This is very similar to the repr
  API.
- Fixed a bug in the script loader that failed in some scenarios when a script
  tag did not have a "src" attribute (thanks Ian!)
- Added new MochiKit.DOM createDOMFunc aliases: H1, H2, H3, BR, HR, TEXTAREA,
  P, FORM
- Use encodeURIComponent / decodeURIComponent for MochiKit.Base urlEncode
  and parseQueryString, when available.

2005-08-12      v0.80

- Source highlighting in all examples, moved to a view-source example
- Added some experimental syntax highlighting for the Rounded Corners example,
  via the LGPL dp.SyntaxHighlighter 1.2.0 now included in examples/common/lib
- Use an indirect binding for the logger conveniences, so that the global
  logger could be replaced by setting MochiKit.Logger.logger to something else
  (though an observer is probably a better choice).
- Allow MochiKit.DOM.getElementsByTagAndClassName to take a string for parent,
  which will be looked up with getElement
- Fixed bug in MochiKit.Color.fromBackground (was using node.parent instead of
  node.parentNode)
- Consider a 304 (NOT_MODIFIED) response from XMLHttpRequest to be success
- Disabled Mozilla map(...) fast-path due to Deer Park compatibility issues
- Possible workaround for Safari issue with swapDOM, where it would get
  confused because two elements were in the DOM at the same time with the
  same id
- Added missing THEAD convenience function to MochiKit.DOM
- Added lstrip, rstrip, strip to MochiKit.Format
- Added updateNodeAttributes, appendChildNodes, replaceChildNodes to
  MochiKit.DOM
- MochiKit.Iter.iextend now has a fast-path for array-like objects
- Added HSV color space support to MochiKit.Visual
- Fixed a bug in the sortable_tables example, it now converts types
  correctly
- Fixed a bug where MochiKit.DOM referenced MochiKit.Iter.next from global
  scope

2005-08-04      v0.70

- New ajax_tables example, which shows off XMLHttpRequest, ajax, json, and
  a little TAL-ish DOM templating attribute language.
- sendXMLHttpRequest and functions that use it (loadJSONDoc, etc.) no longer
  ignore requests with status == 0, which seems to happen for cached or local
  requests
- Added sendXMLHttpRequest to MochiKit.Async.EXPORT, d'oh.
- Changed scrapeText API to return a string by default. This is API-breaking!
  It was dumb to have the default return value be the form you almost never
  want. Sorry.
- Added special form to swapDOM(dest, src). If src is null, dest is removed
  (where previously you'd likely get a DOM exception).
- Added three new functions to MochiKit.Base for dealing with URL query
  strings: urlEncode, queryString, parseQueryString
- MochiKit.DOM.createDOM will now use attr[k] = v for all browsers if the name
  starts with "on" (e.g. "onclick"). If v is a string, it will set it to
  new Function(v).
- Another workaround for Internet "worst browser ever" Explorer's setAttribute
  usage in MochiKit.DOM.createDOM (checked -> defaultChecked).
- Added UL, OL, LI convenience createDOM aliases to MochiKit.DOM
- Packing is now done by Dojo's custom Rhino interpreter, so it's much smaller
  now!

2005-07-29      v0.60

- Beefed up the MochiKit.DOM test suite
- Fixed return value for MochiKit.DOM.swapElementClass, could return
  false unexpectedly before
- Added an optional "parent" argument to
  MochiKit.DOM.getElementsByTagAndClassName
- Added a "packed" version in packed/MochiKit/MochiKit.js
- Changed build script to rewrite the URLs in tests to account for the
  JSAN-required reorganization
- MochiKit.Compat to potentially work around IE 5.5 issues
  (5.0 still not supported). Test.Simple doesn't seem to work there,
  though.
- Several minor documentation corrections

2005-07-27      v0.50

- Initial Release
