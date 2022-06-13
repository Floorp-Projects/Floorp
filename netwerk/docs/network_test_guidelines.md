# Networking Test Guidelines

This is a high level document to introduce the different test types that are used in necko. The target audience is newcomer of necko team.

## Necko Test Types

We only introduce tests under [netwerk/test](https://searchfox.org/mozilla-central/source/netwerk/test) folder in this section.

- [Chrome Tests](https://firefox-source-docs.mozilla.org/testing/chrome-tests/index.html)
  - We usually write chrome tests when the code to be tested needs a browser windows to load some particular resources.
  - Path: [netwerk/test/browser](https://searchfox.org/mozilla-central/source/netwerk/test/browser)
- [Reftest](https://firefox-source-docs.mozilla.org/testing/webrender/index.html)
  - Rarely used in necko.
- [Mochitest](https://firefox-source-docs.mozilla.org/testing/mochitest-plain/index.html)
  - Used when the code to be tested can be triggered by WebIDL. e.g., WebSocket and XMLHttpRequest.
  - Path: [netwerk/test/mochitests](https://searchfox.org/mozilla-central/source/netwerk/test/mochitests)
- [XPCShell tests](https://firefox-source-docs.mozilla.org/testing/xpcshell/index.html#xpcshell-tests)
  - Mostly used in necko to test objects that can be accessed by JS. e.g., `nsIHttpChannel`.
  - Path: [netwerk/test/unit](https://searchfox.org/mozilla-central/source/netwerk/test/unit)
- [GTest](https://firefox-source-docs.mozilla.org/gtest/index.html)
  - Useful when the code doesn't need a http server.
  - Useful when writing code regarding to parsing strings. e.g., [Parsing Server Timing Header](https://searchfox.org/mozilla-central/rev/0249c123e74640ed91edeabba00649ef4d929372/netwerk/test/gtest/TestServerTimingHeader.cpp)
- [Performance tests](https://firefox-source-docs.mozilla.org/testing/perfdocs/index.html)
  - Current tests in [netwerk/test/perf](https://searchfox.org/mozilla-central/source/netwerk/test/perf) are all for testing `HTTP/3` code.

There are also [web-platform-tests](https://firefox-source-docs.mozilla.org/web-platform/index.html) that is related to necko. We don't usually write new `web-platform-tests`. However, we do have lots of `web-platform-tests` for XHR, Fetch, and WebSocket.

## Running Necko xpcshell-tests

- Local:

  Run all xpcshell-tests:

  ```console
  ./mach xpcshell-test netwerk/test/unit
  ```

  Note that xpcshell-tests are run in parallel, sometimes we want to run them sequentially for debugging.

  ```console
  ./mach xpcshell-test --sequential netwerk/test/unit
  ```

  Run a single tests:

  ```console
  ./mach xpcshell-test netwerk/test/unit/test_http3.js
  ```

  Run with socket process enabled:

  ```console
  ./mach xpcshell-test --setpref="network.http.network_access_on_socket_process.enabled=true" netwerk/test/unit/test_http3.js
  ```

  We usually debug networking issues with `HTTP Logging`. To enable logging when running tests:

  ```console
  MOZ_LOG=nsHttp:5 ./mach xpcshell-test netwerk/test/unit/test_http3.js
  ```

- Remote:

  First of all, we need to know [Fuzzy Selector](https://firefox-source-docs.mozilla.org/tools/try/selectors/fuzzy.html), which is the tool we use to select which test to run on try. If you already know that your code change can be covered by necko xpcshell-tests, you can use the following command to run all tests in `netwerk/test/unit` on try.

  ```console
  ./mach try fuzzy netwerk/test/unit
  ```

  Run a single test on try:

  ```console
  ./mach try fuzzy netwerk/test/unit/test_http3.js
  ```

  Sometimes we want to debug the failed test on try with logging enabled:

  ```console
  ./mach try fuzzy --env "MOZ_LOG=nsHttp:5,nsHostResolver:5" netwerk/tesst/unit/test_http3.js
  ```

  Note that it's not usually a good idea to enabling logging when running all tests in a folder on try, since the raw log file can be really huge. The log file might be not available if the size exceeds the limit on try.
  In the case that your code change is too generic or you are not sure which tests to run, you can use [Auto Selector](https://firefox-source-docs.mozilla.org/tools/try/selectors/auto.html) to let it select tests for you.

## Debugging intermittent test failures

There are a lot of intermittent failures on try (usually not able to reproduce locally). Debugging these failures can be really annoying and time consuming. Here are some general guidelines to help you debug intermittent failures more efficiently.

- Identify whether the failure is caused by your code change.
  - Try to reproduce the intermittent failure locally. This is the most straightforward way. Adding `--verify` flag is also helpful when debugging locally (see this [document](https://firefox-source-docs.mozilla.org/testing/test-verification/index.html) for more details).
  - We can check the failure summery on try to see if there is already a bug filed for this test failure. If yes, it's likely this is not caused by your code change.
  - Re-trigger the failed test a few times and see if it passed. This can be easily done by clicking the `Push Health` button.
  - Looking for similar failures happening now on other submissions. This can be done by:
  ```
  click on failing job -> read failure summary -> find similar ones by other authors in similar jobs
  ```
  - To re-run the failed test suite more times, you could add `rebuild` option to `./mach try`. For example, the following command allows to run necko xpcshell-tests 20 times on try.
  ```
  ./mach try fuzzy netwerk/test/unit --rebuild 20
  ```
- In the case that we really need to debug an intermittent test failure, see this [document](https://firefox-source-docs.mozilla.org/devtools/tests/debugging-intermittents.html) first for some general tips. Unfortunately, there is no easy way to debug this. One can try to isolate the failed test first and enable `HTTP logging` on try to collect the log for further analysis.

## Writing Necko XPCShell tests

The most typical form of necko xpcsehll-test is creating an HTTP server and test your code by letting the server return some specific responses (e.g., `103 Early Hint`). We will only introduce how to write this kind of test in this document.

- Code at server side

  After [bug 1756557](https://bugzilla.mozilla.org/show_bug.cgi?id=1756557), it is possible to create a `nodejs` HTTP server in your test code. This saves us some time for writing code at the server side by reusing the HTTP module provided by `nodejs`.
  This is what it looks like to create a simple HTTP server:

  ```js
  let server = new NodeHTTPServer();
  await server.start();
  registerCleanupFunction(async () => {
    await server.stop();
  });
  await server.registerPathHandler("/test", (req, resp) => {
    resp.writeHead(200);
    resp.end("done");
  });
  ```

  We can also create a `HTTP/2` server easily by replacing `NodeHTTPServer` with `NodeHTTP2Server` and adding the server certification.

  ```js
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");
  let server = new NodeHTTP2Server();
  ```

- Code at client side

  The recommend way is to create and open an HTTP channel and handle the response with a `Promise` asynchronously.
  The code would be like:

  ```js
  function makeChan(uri) {
    let chan = NetUtil.newChannel({
      uri,
      loadUsingSystemPrincipal: true,
    }).QueryInterface(Ci.nsIHttpChannel);
    chan.loadFlags = Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
    return chan;
  }
  let chan = makeChan(`http://localhost:${server.port()}/test`);
  let req = await new Promise(resolve => {
    chan.asyncOpen(new ChannelListener(resolve, null, CL_ALLOW_UNKNOWN_CL));
  });
  ```

  This is what it looks like to put everything together:

  ```js
  add_task(async function test_http() {
    let server = new NodeHTTPServer();
    await server.start();
    registerCleanupFunction(async () => {
      await server.stop();
    });
    await server.registerPathHandler("/test", (req, resp) => {
      resp.writeHead(200);
      resp.end("done");
    });
    let chan = makeChan(`http://localhost:${server.port()}/test`);
    let req = await new Promise(resolve => {
      chan.asyncOpen(new ChannelListener(resolve, null, CL_ALLOW_UNKNOWN_CL));
    });
    equal(req.status, Cr.NS_OK);
    equal(req.QueryInterface(Ci.nsIHttpChannel).responseStatus, 200);
    equal(req.QueryInterface(Ci.nsIHttpChannel).protocolVersion, "http/1.1");
  });
  ```
