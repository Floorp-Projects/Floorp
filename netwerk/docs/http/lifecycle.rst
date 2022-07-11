The lifecycle of a HTTP request
===============================


HTTP requests in Firefox go through several steps.  Each piece of the request message and response message become available at certain points.  Extracting that information is a challenge, though.

What is available when
----------------------

+-----------------------+---------------------------------------------------+---------------------------------------+------------------------+-------------------------------+
| Data                  | When it's available                               | Sample JS code                        | Interfaces             | Test code                     |
+=======================+===================================================+=======================================+========================+===============================+
| HTTP request method   | *http-on-modify-request* observer notification    | channel.requestMethod                 | nsIHttpChannel_        |                               |
+-----------------------+---------------------------------------------------+---------------------------------------+------------------------+-------------------------------+
| HTTP request URI      | *http-on-modify-request* observer notification    | channel.URI                           | nsIChannel_            |                               |
+-----------------------+---------------------------------------------------+---------------------------------------+------------------------+-------------------------------+
| HTTP request headers  | *http-on-modify-request* observer notification    | channel.visitRequestHeaders(visitor)  | nsIHttpChannel_        |                               |
+-----------------------+---------------------------------------------------+---------------------------------------+------------------------+-------------------------------+
| HTTP request body     | *http-on-modify-request* observer notification    | channel.uploadStream                  | nsIUploadChannel_      |                               |
+-----------------------+---------------------------------------------------+---------------------------------------+------------------------+-------------------------------+
|| HTTP response status || *http-on-examine-response* observer notification || channel.responseStatus               || nsIHttpChannel_       || test_basic_functionality.js_ |
||                      ||                                                  || channel.responseStatusText           ||                       ||                              |
+-----------------------+---------------------------------------------------+---------------------------------------+------------------------+-------------------------------+
| HTTP response headers | *http-on-examine-response* observer notification  | channel.visitResponseHeaders(visitor) | nsIHttpChannel_        |                               |
+-----------------------+---------------------------------------------------+---------------------------------------+------------------------+-------------------------------+
|| HTTP response body   || *onStopRequest* via stream listener tee          || See below                            || nsITraceableChannel_  || test_traceable_channel.js_   |
||                      ||                                                  ||                                      || nsIStreamListenerTee_ ||                              |
||                      ||                                                  ||                                      || nsIPipe_              ||                              |
+-----------------------+---------------------------------------------------+---------------------------------------+------------------------+-------------------------------+

The request: http-on-modify-request
-----------------------------------

Firefox fires a "http-on-modify-request" observer notification before sending the HTTP request, and this blocks the sending of the request until all observers exit.  This is generally the point at which you can modify the HTTP request headers (hence the name).

Attaching a listener for a request is pretty simple::

  const obs = {
    QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),

    observe: function(channel, topic, data) {
      if (!(channel instanceof Ci.nsIHttpChannel))
        return;

      // process the channel's data
    }
  }

  Services.obs.addObserver(observer, "http-on-modify-request", false);

See nsIObserverService_ for the details.

The request method and URI are immediately available at this time.  Request headers are trivially easy to get::

  /**
   * HTTP header visitor.
   */
  class HeaderVisitor {
    #targetObject;

    constructor(targetObject) {
      this.#targetObject = targetObject;
    }

    // nsIHttpHeaderVisitor
    visitHeader(header, value) {
      this.#targetObject[header] = value;
    }

    QueryInterface = ChromeUtils.generateQI(["nsIHttpHeaderVisitor"]);
  }

  // ...
  const requestHeaders = {};
  const visitor = new HeaderVisitor(requestHeaders);
  channel.visitRequestHeaders(visitor);

This is also the time to set request headers, if you need to.  The method for that on the nsIHttpChannel_ interface is `channel.setRequestHeader(header, value);`

Most HTTP requests don't have a body, as they are GET requests.  POST requests often have them, though.  As the nsIUploadChannel_ documentation indicates, the body of most HTTP requests is available via a seekable stream (nsISeekableStream_).  So you can simply capture the body stream and its current position, to revisit it later.  network-helper.js_ has code to read the request body.

The response: http-on-examine-response
--------------------------------------

Firefox fires a "http-on-examine-response" observer notification after parsing the HTTP response status and headers, but **before** reading the response body.  Attaching a listener for this phase is also very easy::

  Services.obs.addObserver(observer, "http-on-examine-response", false);

If you use the same observer for "http-on-modify-request" and "http-on-examine-response", make sure you check the topic argument before interacting with the channel.

The response status is available via the *responseStatus* and *responseStatusText* properties.  The response headers are available via the *visitResponseHeaders* method, and requires the same interface.

The response body: onStopRequest, stream listener tee
-----------------------------------------------------

During the "http-on-examine-response" notification, the response body is *not* available.  You can, however, use a stream listener tee to *copy* the stream so that the original stream data goes on, and you have a separate input stream you can read from with the same data.

Here's some sample code to illustrate what you need::

  const Pipe = Components.Constructor(
    "@mozilla.org/pipe;1",
    "nsIPipe",
    "init"
  );
  const StreamListenerTee = Components.Constructor(
    "@mozilla.org/network/stream-listener-tee;1",
    "nsIStreamListenerTee"
  );
  const ScriptableStream = Components.Constructor(
    "@mozilla.org/scriptableinputstream;1",
    "nsIScriptableInputStream",
    "init"
  );

  const obs = {
    QueryInterface: ChromeUtils.generateQI(["nsIObserver", "nsIRequestObserver"]),

    /** @typedef {WeakMap<nsIHttpChannel, nsIPipe>} */
    requestToTeePipe: new WeakMap,

    // nsIObserver
    observe: function(channel, topic, data) {
      if (!(channel instanceof Ci.nsIHttpChannel))
        return;

      /* Create input and output streams to take the new data.
         The 0xffffffff argument is the segment count.
         It has to be this high because you don't know how much data is coming in the response body.

         As for why these are blocking streams:  I believe this is because there's no actual need to make them non-blocking.
         The stream processing happens during onStopRequest(), so we have all the data then and the operation can be synchronous.
         But I could be very wrong on this.
      */
      const pipe = new Pipe(false, false, 0, 0xffffffff);

      // Install the stream listener tee to intercept the HTTP body.
      const tee = new StreamListenerTee;
      const originalListener = channel.setNewListener(tee);
      tee.init(originalListener, pipe.outputStream, this);

      this.requestToTeePipe.set(channel, pipe);
    }

    // nsIRequestObserver
    onStartRequest: function() {
      // do nothing
    }

    // nsIRequestObserver
    onStopRequest: function(channel, statusCode) {
      const pipe = this.requestToTeePipe.get(channel);

      // No more data coming in anyway.
      pipe.outputStream.close();
      this.requestToTeePipe.delete(channel);

      let length = 0;
      try {
        length = pipe.inputStream.available();
      }
      catch (e) {
        if (e.result === Components.results.NS_BASE_STREAM_CLOSED)
          throw e;
      }

      let responseBody = "";
      if (length) {
        // C++ code doesn't need the scriptable input stream.
        const sin = new ScriptableStream(pipe.inputStream);
        responseBody = sin.read(length);
        sin.close();
      }

      void(responseBody); // do something with the body
    }
  }

test_traceable_channel.js_ does essentially this.

Character encodings and compression
-----------------------------------

Canceling requests
------------------

HTTP activity distributor notes
-------------------------------

URIContentLoader notes
----------------------

Order of operations
-------------------

1. The HTTP channel is constructed.
2. The "http-on-modify-request" observer service notification fires.
3. If the request has been canceled, exit at this step.
4. The HTTP channel's request is submitted to the server.  Time passes.
5. The HTTP channel's response comes in from the server.
6. The HTTP channel parses the response status and headers.
7. The "http-on-examine-response" observer service notification fires.

Useful code samples and references
----------------------------------

- nsIHttpProtocolHandler_ defines a lot of observer topics, and has a lot of details.

.. _nsIHttpChannel: https://searchfox.org/mozilla-central/source/netwerk/protocol/http/nsIHttpChannel.idl
.. _nsIChannel: https://searchfox.org/mozilla-central/source/netwerk/base/nsIChannel.idl
.. _nsIUploadChannel: https://searchfox.org/mozilla-central/source/netwerk/base/nsIUploadChannel.idl
.. _nsITraceableChannel: https://searchfox.org/mozilla-central/source/netwerk/base/nsITraceableChannel.idl
.. _nsISeekableStream: https://searchfox.org/mozilla-central/source/xpcom/io/nsISeekableStream.idl
.. _nsIObserverService: https://searchfox.org/mozilla-central/source/xpcom/ds/nsIObserverService.idl
.. _nsIHttpProtocolHandler: https://searchfox.org/mozilla-central/source/netwerk/protocol/http/nsIHttpProtocolHandler.idl
.. _nsIStreamListenerTee: https://searchfox.org/mozilla-central/source/netwerk/base/nsIStreamListenerTee.idl
.. _nsIPipe: https://searchfox.org/mozilla-central/source/xpcom/io/nsIPipe.idl

.. _test_basic_functionality.js: https://searchfox.org/mozilla-central/source/netwerk/test/httpserver/test/test_basic_functionality.js
.. _test_traceable_channel.js: https://searchfox.org/mozilla-central/source/netwerk/test/unit/test_traceable_channel.js
.. _network-helper.js: https://searchfox.org/mozilla-central/source/devtools/shared/webconsole/network-helper.js
