/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { JSONHandler } = ChromeUtils.importESModule(
  "chrome://remote/content/cdp/JSONHandler.sys.mjs"
);

// Get list of supported routes from JSONHandler
const routes = new JSONHandler().routes;

add_task(async function json_version() {
  const { userAgent } = Cc[
    "@mozilla.org/network/protocol;1?name=http"
  ].getService(Ci.nsIHttpProtocolHandler);

  const json = await requestJSON("/json/version");
  is(
    json.Browser,
    `${Services.appinfo.name}/${Services.appinfo.version}`,
    "Browser name and version found"
  );
  is(json["Protocol-Version"], "1.3", "Protocol version found");
  is(json["User-Agent"], userAgent, "User agent found");
  is(json["V8-Version"], "1.0", "V8 version found");
  is(json["WebKit-Version"], "1.0", "Webkit version found");
  is(
    json.webSocketDebuggerUrl,
    RemoteAgent.cdp.targetList.getMainProcessTarget().wsDebuggerURL,
    "Websocket URL for main process target found"
  );
});

add_task(async function check_routes() {
  for (const route in routes) {
    const { parameter, method } = routes[route];
    // Skip routes expecting parameter
    if (parameter) {
      continue;
    }

    // Check request succeeded (200) and responded with valid JSON
    info(`Checking ${route}`);
    await requestJSON(route, { method });

    // Check with trailing slash
    info(`Checking ${route + "/"}`);
    await requestJSON(route + "/", { method });

    // Test routes expecting a certain method
    if (method) {
      const responseText = await requestJSON(route, {
        method: "DELETE",
        status: 405,
        json: false,
      });
      is(
        responseText,
        `Using unsafe HTTP verb DELETE to invoke ${route}. This action supports only ${method} verb.`,
        "/json/new fails with 405 when using GET"
      );
    }
  }
});

add_task(async function json_list({ client }) {
  const { Target } = client;
  const { targetInfos } = await Target.getTargets();

  const json = await requestJSON("/json/list");
  const jsonAlias = await requestJSON("/json");

  Assert.deepEqual(json, jsonAlias, "/json/list and /json return the same");

  ok(Array.isArray(json), "Target list is an array");

  is(
    json.length,
    targetInfos.length,
    "Targets as listed on /json/list are equal to Target.getTargets"
  );

  for (let i = 0; i < json.length; i++) {
    const jsonTarget = json[i];
    const wsTarget = targetInfos[i];

    is(
      jsonTarget.id,
      wsTarget.targetId,
      "Target id matches between HTTP and Target.getTargets"
    );
    is(
      jsonTarget.type,
      wsTarget.type,
      "Target type matches between HTTP and Target.getTargets"
    );
    is(
      jsonTarget.url,
      wsTarget.url,
      "Target url matches between HTTP and Target.getTargets"
    );

    // Ensure expected values specifically for JSON endpoint
    // and that type is always "page" as main process target should not be included
    is(
      jsonTarget.type,
      "page",
      `Target (${jsonTarget.id}) from list has expected type (page)`
    );
    is(
      jsonTarget.webSocketDebuggerUrl,
      `ws://${RemoteAgent.debuggerAddress}/devtools/page/${wsTarget.targetId}`,
      `Target (${jsonTarget.id}) from list has expected webSocketDebuggerUrl value`
    );
  }
});

add_task(async function json_new_target({ client }) {
  const newUrl = "https://example.com";

  let getError = await requestJSON("/json/new?" + newUrl, {
    status: 405,
    json: false,
  });
  is(
    getError,
    "Using unsafe HTTP verb GET to invoke /json/new. This action supports only PUT verb.",
    "/json/new fails with 405 when using GET"
  );

  const newTarget = await requestJSON("/json/new?" + newUrl, { method: "PUT" });

  is(newTarget.type, "page", "Returned target type is 'page'");
  is(newTarget.url, newUrl, "Returned target URL matches");
  ok(!!newTarget.id, "Returned target has id");
  ok(
    !!newTarget.webSocketDebuggerUrl,
    "Returned target has webSocketDebuggerUrl"
  );

  const { Target } = client;
  const targets = await getDiscoveredTargets(Target);
  const foundTarget = targets.find(target => target.targetId === newTarget.id);

  ok(!!foundTarget, "Returned target id was found");
});

add_task(async function json_activate_target({ client, tab }) {
  const { Target, target } = client;

  const currentTargetId = target.id;
  const targets = await getDiscoveredTargets(Target);
  const initialTarget = targets.find(
    target => target.targetId === currentTargetId
  );
  ok(!!initialTarget, "The current target has been found");

  // open some more tabs in the initial window
  await openTab(Target);
  await openTab(Target);

  const lastTabFirstWindow = await openTab(Target);
  is(
    gBrowser.selectedTab,
    lastTabFirstWindow.newTab,
    "Selected tab has changed to a new tab"
  );

  const activateResponse = await requestJSON(
    "/json/activate/" + initialTarget.targetId,
    { json: false }
  );

  is(
    activateResponse,
    "Target activated",
    "Activate endpoint returned expected string"
  );

  is(gBrowser.selectedTab, tab, "Selected tab is the initial tab again");

  const invalidResponse = await requestJSON("/json/activate/does-not-exist", {
    status: 404,
    json: false,
  });

  is(invalidResponse, "No such target id: does-not-exist");
});

add_task(async function json_close_target({ CDP, client }) {
  const { Target } = client;

  const { targetInfo, newTab } = await openTab(Target);

  const targetListBefore = await CDP.List();
  const beforeTarget = targetListBefore.find(
    target => target.id === targetInfo.targetId
  );

  ok(!!beforeTarget, "New target has been found");

  const tabClosed = BrowserTestUtils.waitForEvent(newTab, "TabClose");
  const targetDestroyed = Target.targetDestroyed();

  const activateResponse = await requestJSON(
    "/json/close/" + targetInfo.targetId,
    { json: false }
  );
  is(
    activateResponse,
    "Target is closing",
    "Close endpoint returned expected string"
  );

  await tabClosed;
  info("Tab was closed");

  await targetDestroyed;
  info("Received the Target.targetDestroyed event");

  const targetListAfter = await CDP.List();
  const afterTarget = targetListAfter.find(
    target => target.id === targetInfo.targetId
  );

  Assert.equal(afterTarget, null, "New target is gone");

  const invalidResponse = await requestJSON("/json/close/does-not-exist", {
    status: 404,
    json: false,
  });

  is(invalidResponse, "No such target id: does-not-exist");
});

add_task(async function json_prevent_load_in_iframe({ client }) {
  const { Page } = client;

  const PAGE = `https://example.com/document-builder.sjs?html=${encodeURIComponent(
    '<iframe src="http://localhost:9222/json/version"></iframe>`'
  )}`;

  await Page.enable();

  const NAVIGATED = "Page.frameNavigated";

  const history = new RecordEvents(2);
  history.addRecorder({
    event: Page.frameNavigated,
    eventName: NAVIGATED,
    messageFn: payload => {
      return `Received ${NAVIGATED} for frame id ${payload.frame.id}`;
    },
  });

  await loadURL(PAGE);

  const frameNavigatedEvents = await history.record();

  const frames = frameNavigatedEvents
    .map(({ payload }) => payload.frame)
    .filter(frame => frame.parentId !== undefined);

  const windowGlobal = BrowsingContext.get(frames[0].id).currentWindowGlobal;
  ok(
    windowGlobal.documentURI.spec.startsWith("about:neterror?e=cspBlocked"),
    "Expected page not be loaded within an iframe"
  );
});

async function requestJSON(path, options = {}) {
  const { method = "GET", status = 200, json = true } = options;

  info(`${method} http://${RemoteAgent.debuggerAddress}${path}`);

  const response = await fetch(`http://${RemoteAgent.debuggerAddress}${path}`, {
    method,
  });

  is(response.status, status, `JSON response is ${status}`);

  if (json) {
    return response.json();
  }

  return response.text();
}
