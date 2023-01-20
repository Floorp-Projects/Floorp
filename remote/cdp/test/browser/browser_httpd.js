/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { JSONHandler } = ChromeUtils.importESModule(
  "chrome://remote/content/cdp/JSONHandler.sys.mjs"
);

// Get list of supported routes from JSONHandler
const routes = Object.keys(new JSONHandler().routes);

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
  for (const route of routes) {
    // Check request succeeded (200) and responded with valid JSON
    info(`Checking ${route}`);
    await requestJSON(route);

    info(`Checking ${route + "/"}`);
    await requestJSON(route + "/");
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

async function requestJSON(path) {
  const response = await fetch(`http://${RemoteAgent.debuggerAddress}${path}`);
  is(response.status, 200, "JSON response is 200");

  return response.json();
}
