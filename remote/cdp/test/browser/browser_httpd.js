/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function json_version() {
  await RemoteAgent.listen(`http://localhost:0`);

  const { userAgent } = Cc[
    "@mozilla.org/network/protocol;1?name=http"
  ].getService(Ci.nsIHttpProtocolHandler);

  const json = await requestJSON("/version");
  is(
    json.Browser,
    `${Services.appinfo.name}/${Services.appinfo.version}`,
    "Browser name and version found"
  );
  is(json["Protocol-Version"], "1.0", "Protocol version found");
  is(json["User-Agent"], userAgent, "User agent found");
  is(json["V8-Version"], "1.0", "V8 version found");
  is(json["WebKit-Version"], "1.0", "Webkit version found");
  is(
    json.webSocketDebuggerUrl,
    RemoteAgent.cdp.targetList.getMainProcessTarget().wsDebuggerURL,
    "Websocket URL for main process target found"
  );
});

function requestJSON(path) {
  const url = `http://${RemoteAgent.debuggerAddress}`;

  return new Promise(resolve => {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", `${url}/json${path}`);
    xhr.responseType = "json";
    xhr.onloadend = () => {
      resolve(xhr.response);
    };
    xhr.send();
  });
}
