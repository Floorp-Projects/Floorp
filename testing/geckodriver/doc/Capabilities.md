Firefox capabilities
====================

geckodriver has a few capabilities that are specific to Firefox.
Most of these [are documented on MDN](https://developer.mozilla.org/en-US/docs/Web/WebDriver/Capabilities/firefoxOptions).

We additionally have some capabilities that largely are implementation
concerns that normal users should not care about:


`moz:useNonSpecCompliantPointerOrigin`
--------------------------------------

A boolean value to indicate how the pointer origin for an action
command will be calculated.

With Firefox 59 the calculation will be based on the requirements
by the [WebDriver] specification. This means that the pointer origin
is no longer computed based on the top and left position of the
referenced element, but on the in-view center point.

To temporarily disable the WebDriver conformant behavior use `false`
as value for this capability.

Please note that this capability exists only temporarily, and that
it will be removed once all Selenium bindings can handle the new
behavior.


`moz:webdriverClick`
--------------------

A boolean value to indicate which kind of interactability checks
to run when performing a click or sending keys to an elements. For
Firefoxen prior to version 58.0 some legacy code as imported from
an older version of FirefoxDriver was in use.

With Firefox 58 the interactability checks as required by the
[WebDriver] specification are enabled by default. This means
geckodriver will additionally check if an element is obscured by
another when clicking, and if an element is focusable for sending
keys.

Because of this change in behaviour, we are aware that some extra
errors could be returned. In most cases the test in question might
have to be updated so it's conform with the new checks. But if the
problem is located in geckodriver, then please raise an issue in
the [issue tracker].

To temporarily disable the WebDriver conformant checks use `false`
as value for this capability.

Please note that this capability exists only temporarily, and that
it will be removed once the interactability checks have been
stabilized.
