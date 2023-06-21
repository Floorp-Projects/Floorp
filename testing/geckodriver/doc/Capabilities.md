# Firefox capabilities

geckodriver has a few capabilities that are specific to Firefox.
Most of these [are documented on MDN](https://developer.mozilla.org/en-US/docs/Web/WebDriver/Capabilities/firefoxOptions).

We additionally have some capabilities that largely are implementation
concerns that normal users should not care about:

## `moz:debuggerAddress`

A boolean value to indicate if Firefox has to be started with the
[Remote Protocol] enabled, which is a low-level debugging interface that
implements a subset of the [Chrome DevTools Protocol] (CDP).

When enabled the returned `moz:debuggerAddress` capability of the `New Session`
command is the `host:port` combination of a server that supports the following
HTTP endpoints:

### GET /json/version

The browser version metadata:

```json
{
    "Browser": "Firefox/84.0a1",
    "Protocol-Version": "1.0",
    "User-Agent": "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:84.0) Gecko/20100101 Firefox/84.0",
    "V8-Version": "1.0",
    "WebKit-Version": "1.0",
    "webSocketDebuggerUrl": "ws://localhost:9222/devtools/browser/fe507083-2960-a442-bbd7-7dfe1f111c05"
}
```

### GET /json/list

A list of all available websocket targets:

```json
[ {
    "description": "",
    "devtoolsFrontendUrl": null,
    "faviconUrl": "",
    "id": "ecbf9028-676a-1b40-8596-a5edc0e2875b",
    "type": "page",
    "url": "https://www.mozilla.org/en-US/",
    "browsingContextId": 29,
    "webSocketDebuggerUrl": "ws://localhost:9222/devtools/page/ecbf9028-676a-1b40-8596-a5edc0e2875b"
} ]
```

The contained `webSocketDebuggerUrl` entries can be used to connect to the
websocket and interact with the browser by using the CDP protocol.

[Remote Protocol]: /remote/index.rst
[Chrome DevTools Protocol]: https://chromedevtools.github.io/devtools-protocol/

## `moz:useNonSpecCompliantPointerOrigin`

Note: This capability is no longer supported in Firefox 116 and later.

A boolean value to indicate how the pointer origin for an action
command will be calculated.

With Firefox 59 the calculation will be based on the requirements
by the [WebDriver] specification. This means that the pointer origin
is no longer computed based on the top and left position of the
referenced element, but on the in-view center point.

To temporarily disable the WebDriver conformant behavior use `false`
as value for this capability.

## `moz:webdriverClick`

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
