# Mochitest FAQ

## SSL and https-enabled tests

Mochitests must be run from http://mochi.test/ to succeed. However, some tests
may require use of additional protocols, hosts, or ports to test cross-origin
functionality.

The Mochitest harness addresses this need by mirroring all content of the
original server onto a variety of other servers through the magic of proxy
autoconfig and SSL tunneling. The full list of schemes, hosts, and ports on
which tests are served, is specified in `build/pgo/server-locations.txt`.

The origins described there are not the same, as some of them specify
particular SSL certificates for testing purposes, while some allow pages on
that server to request elevated privileges; read the file for full details.

It works as follows: The Mochitest harness includes preference values which
cause the browser to use proxy autoconfig to match requested URLs with servers.
The `network.proxy.autoconfig_url` preference is set to a data: URL that
encodes the JavaScript function, `FindProxyForURL`, which determines the host
of the given URL. In the case of SSL sites to be mirrored, the function maps
them to an SSL tunnel, which transparently forwards the traffic to the actual
server, as per the description of the CONNECT method given in RFC 2817. In this
manner a single HTTP server at http://127.0.0.1:8888 can successfully emulate
dozens of servers at distinct locations.

## What if my tests aren't done when onload fires?

Use `add_task()`, or call `SimpleTest.waitForExplicitFinish()` before onload
fires (and `SimpleTest.finish()` when you're done).

## How can I get the full log output for my test in automation for debugging?

Add the following to your test:

```
SimpleTest.requestCompleteLog();
```

## What if I need to change a preference to run my test?

The `SpecialPowers` object provides APIs to get and set preferences:

```js
await SpecialPowers.pushPrefEnv({ set: [["your-preference", "your-value" ]] });
// ...
await SpecialPowers.popPrefEnv(); // Implicit at the end of the test too.
```

You can also set prefs directly in the manifest:

```ini
[DEFAULT]
prefs =
  browser.chrome.guess_favicon=true
```

If you need to change a pref when running a test locally, you can use the
`--setpref` flag:

```
./mach mochitest --setpref="javascript.options.jit.chrome=false" somePath/someTestFile.html
```

Equally, if you need to change a string pref:

```
./mach mochitest --setpref="webgl.osmesa=string with whitespace" somePath/someTestFile.html
```

## Can tests be run under a chrome URL?

Yes, use [mochitest-chrome](../chrome-tests/index.rst).

## How do I change the HTTP headers or status sent with a file used in a Mochitest?

Create a text file next to the file whose headers you want to modify. The name
of the text file should be the name of the file whose headers you're modifying
followed by `^headers^`. For example, if you have a file `foo.jpg`, the
text file should be named `foo.jpg^headers^`. (Don't try to actually use the
headers file in any other way in the test, because the HTTP server's
hidden-file functionality prevents any file ending in exactly one ^ from being
served.)

Edit the file to contain the headers and/or status you want to set, like so:

```
HTTP 404 Not Found
Content-Type: text/html
Random-Header-of-Doom: 17
```

The first line sets the HTTP status and a description (optional) associated
with the file. This line is optional; you don't need it if you're fine with the
normal response status and description.

Any other lines in the file describe additional headers which you want to add
or overwrite (most typically the Content-Type header, for the latter case) on
the response. The format follows the conventions of HTTP, except that you don't
need to have HTTP line endings and you can't use a header more than once (the
last line for a particular header wins). The file may end with at most one
blank line to match Unix text file conventions, but the trailing newline isn't
strictly necessary.

## How do I write tests that check header values, method types, etc. of HTTP requests?

To write such a test, you simply need to write an SJS (server-side JavaScript)
for it. See the [testing HTTP server](/networking/http_server_for_testing.rst)
docs for less mochitest-specific documentation of what you can do in SJS
scripts.

An SJS is simply a JavaScript file with the extension .sjs which is loaded in a
sandbox. Don't forget to reference it from your `mochitest.ini` file too!

```ini
[DEFAULT]
support-files =
  test_file.sjs
```

The global property `handleRequest` defined by the script is then executed with
request and response objects, and the script populates the response based on the
information in the request.

Here's an example of a simple SJS:

```js
function handleRequest(request, response) {
  // Allow cross-origin, so you can XHR to it!
  response.setHeader("Access-Control-Allow-Origin", "*", false);
  // Avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/plain", false);
  response.write("Hello world!");
}
```

The file is run, for example, at either
http://mochi.test:8888/tests/PATH/TO/YOUR/test_file.sjs,
http://{server-location}/tests/PATH/TO/YOUR/test_file.sjs - see
`build/pgo/server-locations.txt` for server locations!

If you want to actually execute the file, you need to reference it somehow. For
instance, you can XHR to it OR you could use a HTML element:

```js
var xhr = new XMLHttpRequest();
xhr.open("GET", "http://test/tests/dom/manifest/test/test_file.sjs");
xhr.onload = function(e){ console.log("loaded!", this.responseText)}
xhr.send();
```

The exact properties of the request and response parameters are defined in the
`nsIHttpRequestMetadata` and `nsIHttpResponse` interfaces in
`nsIHttpServer.idl`. However, here are a few useful ones:


 * `.scheme` (string). The scheme of the request.
 * `.host` (string). The scheme of the request.
 * `.port` (string). The port of the request.
 * `.method` (string). The HTTP method.
 * `.httpVersion` (string). The protocol version, typically "1.1".
 * `.path` (string). Path of the request,
 * `.headers` (object). Name and values representing the headers.
 * `.queryString` (string). The query string of the requested URL.
 * `.bodyInputStream` ??
 * `.getHeader(name)`. Gets a request header by name.
 * `.hasHeader(name)` (boolean). Gets a request header by name.

**Note**: The browser is free to cache responses generated by your script. If
you ever want an SJS to return different data for multiple requests to the same
URL, you should add a `Cache-Control: no-cache` header to the response to
prevent the test from accidentally failing, especially if it's manually run
multiple times in the same Mochitest session.

## How do I keep state across loads of different server-side scripts?

Server-side scripts in Mochitest are run inside sandboxes, with a new sandbox
created for each new load. Consequently, any variables set in a handler don't
persist across loads. To support state storage, use the `getState(k)` and
`setState(k, v)` methods defined on the global object. These methods expose a
key-value storage mechanism for the server, with keys and values as strings.
(Use JSON to store objects and other structured data.) The myriad servers in
Mochitest are in reality a single server with some proxying and tunnelling
magic, so a stored state is the same in all servers at all times.

The `getState` and `setState` methods are scoped to the path being loaded. For
example, the absolute URLs `/foo/bar/baz, /foo/bar/baz?quux, and
/foo/bar/baz#fnord` all share the same state; the state for /foo/bar is entirely
separate.

You should use per-path state whenever possible to avoid inter-test dependencies
and bugs.

However, in rare cases it may be necessary for two scripts to collaborate in
some manner, and it may not be possible to use a custom query string to request
divergent behaviors from the script.

For this use case only you should use the `getSharedState(k, v)` and
`setSharedState(k, v)` methods defined on the global object. No restrictions
are placed on access to this whole-server shared state, and any script may add
new state that any other script may delete. To avoid conflicts, you should use
a key within a faux namespace so as to avoid accidental conflicts. For example,
if you needed shared state for an HTML5 video test, you might use a key like
`dom.media.video:sharedState`.

A further form of state storage is provided by the `getObjectState(k)` and
`setObjectState(k, v)` methods, which will store any `nsISupports` object.
These methods reside on the `nsIHttpServer` interface, but a limitation of
the sandbox object used by the server to process SJS responses means that the
former is present in the SJS request handler's global environment with the
signature `getObjectState(k, callback)`, where callback is a function to be
invoked by `getObjectState` with the object corresponding to the provided key
as the sole argument.

Note that this value mapping requires the value to be an XPCOM object; an
arbitrary JavaScript object with no `QueryInterface` method is insufficient.
If you wish to store a JavaScript object, you may find it useful
to provide the object with a `QueryInterface` implementation and then make
use of `wrappedJSObject` to reveal the actual JavaScript object through the
wrapping performed by XPConnect.

For further details on state-saving mechanisms provided by `httpd.js`, see
`netwerk/test/httpserver/nsIHttpServer.idl` and the
`nsIHttpServer.get(Shared|Object)?State` methods.

## How do I write a SJS script that responds asynchronously?

Sometimes you need to respond to a request asynchronously, for example after
waiting for a short period of time. You can do this by using the
`processAsync()` and `finish()` functions on the response object passed to the
`handleRequest()` function.

`processAsync()` must be called before returning from `handleRequest()`. Once
called, you can at any point call methods on the request object to send
more of the response. Once you are done, call the `finish()` function. For
example you can use the `setState()` / `getState()` functions described above to
store a request and later retrieve and finish it. However be aware that the
browser often reorders requests and so your code must be resilient to that to
avoid intermittent failures.

```js
let { setTimeout } = ChromeUtils.importESModule("resource://gre/modules/Timer.sys.mjs");

function handleRequest(request, response) {
  response.processAsync();
  response.setHeader("Content-Type", "text/plain", false);
  response.write("hello...");

  setTimeout(function() {
    response.write("world!");
    response.finish();
  }, 5 * 1000);
}
```

For more details, see the `processAsync()` function documentation in
`netwerk/test/httpserver/nsIHttpServer.idl`.

## How do I get access to the files on the server as XPCOM objects from an SJS script?

If you need access to a file, because it's easier to store image data in a file
than directly in an SJS script, use the presupplied `SERVER_ROOT` object
state available to SJS scripts running in Mochitest:

```js
function handleRequest(req, res) {
  var file;
  getObjectState("SERVER_ROOT", function(serverRoot) {
    file = serverRoot.getFile("tests/content/media/test/320x240.ogv");
  });
  // file is now an XPCOM object referring to the given file
  res.write("file: " + file);
}
```

The path you specify is used as a path relative to the root directory served by
`httpd.js`, and an `nsIFile` corresponding to the file at that location is
returned.

Beware of typos: the file you specify doesn't actually have to exist
because file objects are mere encapsulations of string paths.

## Diagnosing and fixing leakcheck failures

Mochitests output a log of the windows and docshells that are created during the
test during debug builds. At the end of the test, the test runner runs a
leakcheck analysis to determine if any of them did not get cleaned up before the
test was ended.

Leaks can happen for a variety of reasons. One common one is that a JavaScript
event listener is retaining a reference that keeps the window alive.

```js
// Add an observer.
Services.obs.addObserver(myObserver, "event-name");

// Make sure and clean it up, or it may leak!
Services.obs.removeObserver(myObserver, "event-name");
```

Other sources of issues include accidentally leaving a window, or iframe
attached to the DOM, or setting an iframe's src to a blank string (creating an
about:blank page), rather than removing the iframe.

Finding the leak can be difficult, but the first step is to reproduce it
locally. Ensure you are on a debug build and the `MOZ_QUIET` environment flag
is not enabled. The leakcheck test analyzes the test output. After reproducing
the leak in the test, start commenting out code until the leak goes away. Then
once the leak stop reproducing, find the exact location where it is happening.

See [this post](https://crisal.io/words/2019/11/13/shutdown-leak-hunting.html)
for more advanced debugging techniques involving CC and GC logs.
