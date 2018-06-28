"use strict";

let commonEvents = {
  "onBeforeRequest":     [{urls: ["<all_urls>"]}, ["blocking"]],
  "onBeforeSendHeaders": [{urls: ["<all_urls>"]}, ["blocking", "requestHeaders"]],
  "onSendHeaders":       [{urls: ["<all_urls>"]}, ["requestHeaders"]],
  "onBeforeRedirect":    [{urls: ["<all_urls>"]}],
  "onHeadersReceived":   [{urls: ["<all_urls>"]}, ["blocking", "responseHeaders"]],
  // Auth tests will need to set their own events object
  // "onAuthRequired":      [{urls: ["<all_urls>"]}, ["blocking", "responseHeaders"]],
  "onResponseStarted":   [{urls: ["<all_urls>"]}],
  "onCompleted":         [{urls: ["<all_urls>"]}, ["responseHeaders"]],
  "onErrorOccurred":     [{urls: ["<all_urls>"]}],
};

function background(events) {
  const IP_PATTERN = /^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$/;

  let expect;
  let ignore;
  let defaultOrigin;
  let watchAuth = Object.keys(events).includes("onAuthRequired");
  let expectedIp = null;

  browser.test.onMessage.addListener((msg, expected) => {
    if (msg !== "set-expected") {
      return;
    }
    expect = expected.expect;
    defaultOrigin = expected.origin;
    ignore = expected.ignore;
    let promises = [];
    // Initialize some stuff we'll need in the tests.
    for (let entry of Object.values(expect)) {
      // a place for the test infrastructure to store some state.
      entry.test = {};
      // Each entry in expected gets a Promise that will be resolved in the
      // last event for that entry.  This will either be onCompleted, or the
      // last entry if an events list was provided.
      promises.push(new Promise(resolve => { entry.test.resolve = resolve; }));
      // If events was left undefined, we're expecting all normal events we're
      // listening for, exclude onBeforeRedirect and onErrorOccurred
      if (entry.events === undefined) {
        entry.events = Object.keys(events).filter(name => name != "onErrorOccurred" && name != "onBeforeRedirect");
      }
      if (entry.optional_events === undefined) {
        entry.optional_events = [];
      }
    }
    // When every expected entry has finished our test is done.
    Promise.all(promises).then(() => {
      browser.test.sendMessage("done");
    });
    browser.test.sendMessage("continue");
  });

  // Retrieve the per-file/test expected values.
  function getExpected(details) {
    let url = new URL(details.url);
    let filename;
    if (url.protocol == "data:") {
      // pathname is everything after protocol.
      filename = url.pathname;
    } else {
      filename = url.pathname.split("/").pop();
    }
    if (ignore && ignore.includes(filename)) {
      return;
    }
    let expected = expect[filename];
    if (!expected) {
      browser.test.fail(`unexpected request ${filename}`);
      return;
    }
    // Save filename for redirect verification.
    expected.test.filename = filename;
    return expected;
  }

  // Process any test header modifications that can happen in request or response phases.
  // If a test includes headers, it needs a complete header object, no undefined
  // objects even if empty:
  // request: {
  //   add: {"HeaderName": "value",},
  //   modify: {"HeaderName": "value",},
  //   remove: ["HeaderName",],
  // },
  // response: {
  //   add: {"HeaderName": "value",},
  //   modify: {"HeaderName": "value",},
  //   remove: ["HeaderName",],
  // },
  function processHeaders(phase, expected, details) {
    // This should only happen once per phase [request|response].
    browser.test.assertFalse(!!expected.test[phase], `First processing of headers for ${phase}`);
    expected.test[phase] = true;

    let headers = details[`${phase}Headers`];
    browser.test.assertTrue(Array.isArray(headers), `${phase}Headers array present`);

    let {add, modify, remove} = expected.headers[phase];

    for (let name in add) {
      browser.test.assertTrue(!headers.find(h => h.name === name), `header ${name} to be added not present yet in ${phase}Headers`);
      let header = {name: name};
      if (name.endsWith("-binary")) {
        header.binaryValue = Array.from(add[name], c => c.charCodeAt(0));
      } else {
        header.value = add[name];
      }
      headers.push(header);
    }

    let modifiedAny = false;
    for (let header of headers) {
      if (header.name.toLowerCase() in modify) {
        header.value = modify[header.name.toLowerCase()];
        modifiedAny = true;
      }
    }
    browser.test.assertTrue(modifiedAny, `at least one ${phase}Headers element to modify`);

    let deletedAny = false;
    for (let j = headers.length; j-- > 0;) {
      if (remove.includes(headers[j].name.toLowerCase())) {
        headers.splice(j, 1);
        deletedAny = true;
      }
    }
    browser.test.assertTrue(deletedAny, `at least one ${phase}Headers element to delete`);

    return headers;
  }

  // phase is request or response.
  function checkHeaders(phase, expected, details) {
    if (!/^https?:/.test(details.url)) {
      return;
    }

    let headers = details[`${phase}Headers`];
    browser.test.assertTrue(Array.isArray(headers), `valid ${phase}Headers array`);

    let {add, modify, remove} = expected.headers[phase];
    for (let name in add) {
      let value = headers.find(h => h.name.toLowerCase() === name.toLowerCase()).value;
      browser.test.assertEq(value, add[name], `header ${name} correctly injected in ${phase}Headers`);
    }

    for (let name in modify) {
      let value = headers.find(h => h.name.toLowerCase() === name.toLowerCase()).value;
      browser.test.assertEq(value, modify[name], `header ${name} matches modified value`);
    }

    for (let name of remove) {
      let found = headers.find(h => h.name.toLowerCase() === name.toLowerCase());
      browser.test.assertFalse(!!found, `deleted header ${name} still found in ${phase}Headers`);
    }
  }

  let listeners = {
    onBeforeRequest(expected, details, result) {
      // Save some values to test request consistency in later events.
      browser.test.assertTrue(details.tabId !== undefined, `tabId ${details.tabId}`);
      browser.test.assertTrue(details.requestId !== undefined, `requestId ${details.requestId}`);
      // Validate requestId if it's already set, this happens with redirects.
      if (expected.test.requestId !== undefined) {
        browser.test.assertEq("string", typeof expected.test.requestId, `requestid ${expected.test.requestId} is string`);
        browser.test.assertEq("string", typeof details.requestId, `requestid ${details.requestId} is string`);
        browser.test.assertEq("number", typeof parseInt(details.requestId, 10), "parsed requestid is number");
        browser.test.assertEq(expected.test.requestId, details.requestId, "redirects will keep the same requestId");
      } else {
        // Save any values we want to validate in later events.
        expected.test.requestId = details.requestId;
        expected.test.tabId = details.tabId;
      }
      // Tests we don't need to do every event.
      browser.test.assertTrue(details.type.toUpperCase() in browser.webRequest.ResourceType, `valid resource type ${details.type}`);
      if (details.type == "main_frame") {
        browser.test.assertEq(0, details.frameId, "frameId is zero when type is main_frame, see bug 1329299");
      }
    },
    onBeforeSendHeaders(expected, details, result) {
      if (expected.headers && expected.headers.request) {
        result.requestHeaders = processHeaders("request", expected, details);
      }
      if (expected.redirect) {
        browser.test.log(`${name} redirect request`);
        result.redirectUrl = details.url.replace(expected.test.filename, expected.redirect);
      }
    },
    onBeforeRedirect() {},
    onSendHeaders(expected, details, result) {
      if (expected.headers && expected.headers.request) {
        checkHeaders("request", expected, details);
      }
    },
    onResponseStarted() {},
    onHeadersReceived(expected, details, result) {
      let expectedStatus = expected.status || 200;
      // If authentication is being requested we don't fail on the status code.
      if (watchAuth && [401, 407].includes(details.statusCode)) {
        expectedStatus = details.statusCode;
      }
      browser.test.assertEq(expectedStatus, details.statusCode,
                            `expected HTTP status received for ${details.url} ${details.statusLine}`);
      if (expected.headers && expected.headers.response) {
        result.responseHeaders = processHeaders("response", expected, details);
      }
    },
    onAuthRequired(expected, details, result) {
      result.authCredentials = expected.authInfo;
    },
    onCompleted(expected, details, result) {
      // If we have already completed a GET request for this url,
      // and it was found, we expect for the response to come fromCache.
      // expected.cached may be undefined, force boolean.
      if (typeof expected.cached === "boolean") {
        let expectCached = expected.cached && details.method === "GET" && details.statusCode != 404;
        browser.test.assertEq(expectCached, details.fromCache, "fromCache is correct");
      }
      // We can only tell IPs for non-cached HTTP requests.
      if (!details.fromCache && /^https?:/.test(details.url)) {
        browser.test.assertTrue(IP_PATTERN.test(details.ip), `IP for ${details.url} looks IP-ish: ${details.ip}`);

        // We can't easily predict the IP ahead of time, so just make
        // sure they're all consistent.
        expectedIp = expectedIp || details.ip;
        browser.test.assertEq(expectedIp, details.ip, `correct ip for ${details.url}`);
      }
      if (expected.headers && expected.headers.response) {
        checkHeaders("response", expected, details);
      }
    },
    onErrorOccurred(expected, details, result) {
      if (expected.error) {
        browser.test.assertEq(expected.error, details.error, "expected error message received in onErrorOccurred");
      }
    },
  };

  function getListener(name) {
    return details => {
      let result = {};
      browser.test.log(`${name} ${details.requestId} ${details.url}`);
      let expected = getExpected(details);
      if (!expected) {
        return result;
      }
      let expectedEvent = expected.events[0] == name;
      if (expectedEvent) {
        expected.events.shift();
      } else {
        // e10s vs. non-e10s errors can end with either onCompleted or onErrorOccurred
        expectedEvent = expected.optional_events.includes(name);
      }
      browser.test.assertTrue(expectedEvent, `received ${name}`);
      browser.test.assertEq(expected.type, details.type, "resource type is correct");
      browser.test.assertEq(expected.origin || defaultOrigin, details.originUrl, "origin is correct");
      // ignore origin test for generated background page
      if (!details.originUrl || !details.originUrl.endsWith("_generated_background_page.html")) {
        browser.test.assertEq(expected.origin || defaultOrigin, details.originUrl, "origin is correct");
      }

      if (name != "onBeforeRequest") {
        // On events after onBeforeRequest, check the previous values.
        browser.test.assertEq(expected.test.requestId, details.requestId, "correct requestId");
        browser.test.assertEq(expected.test.tabId, details.tabId, "correct tabId");
      }
      try {
        listeners[name](expected, details, result);
      } catch (e) {
        browser.test.fail(`unexpected webrequest failure ${name} ${e}`);
      }

      if (expected.cancel && expected.cancel == name) {
        browser.test.log(`${name} cancel request`);
        browser.test.sendMessage("cancelled");
        result.cancel = true;
      }
      // If we've used up all the events for this test, resolve the promise.
      // If something wrong happens and more events come through, there will be
      // failures.
      if (expected.events.length <= 0) {
        expected.test.resolve();
      }
      return result;
    };
  }

  for (let [name, args] of Object.entries(events)) {
    browser.test.log(`adding listener for ${name}`);
    try {
      browser.webRequest[name].addListener(getListener(name), ...args);
    } catch (e) {
      browser.test.assertTrue(/\brequestBody\b/.test(e.message),
                              "Request body is unsupported");

      // RequestBody is disabled in release builds.
      if (!/\brequestBody\b/.test(e.message)) {
        throw e;
      }

      args.splice(args.indexOf("requestBody"), 1);
      browser.webRequest[name].addListener(getListener(name), ...args);
    }
  }
}

/* exported makeExtension */

function makeExtension(events = commonEvents) {
  return ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: [
        "webRequest",
        "webRequestBlocking",
        "<all_urls>",
      ],
    },
    background: `(${background})(${JSON.stringify(events)})`,
  });
}

/* exported addStylesheet */

function addStylesheet(file) {
  let link = document.createElement("link");
  link.setAttribute("rel", "stylesheet");
  link.setAttribute("href", file);
  document.body.appendChild(link);
}

/* exported addLink */

function addLink(file) {
  let a = document.createElement("a");
  a.setAttribute("href", file);
  a.setAttribute("target", "_blank");
  document.body.appendChild(a);
  return a;
}

/* exported addImage */

function addImage(file) {
  let img = document.createElement("img");
  img.setAttribute("src", file);
  document.body.appendChild(img);
}

/* exported addScript */

function addScript(file) {
  let script = document.createElement("script");
  script.setAttribute("type", "text/javascript");
  script.setAttribute("src", file);
  document.getElementsByTagName("head").item(0).appendChild(script);
}

/* exported addFrame */

function addFrame(file) {
  let frame = document.createElement("iframe");
  frame.setAttribute("width", "200");
  frame.setAttribute("height", "200");
  frame.setAttribute("src", file);
  document.body.appendChild(frame);
}
