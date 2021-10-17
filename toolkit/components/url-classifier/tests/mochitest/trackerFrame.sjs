/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.importGlobalProperties(["URLSearchParams"]);

const stateTotalRequests = "total-request";
const stateCallback = "callback-response";
const stateTrackersWithCookie = "trackers-with-cookie";
const stateTrackersWithoutCookie = "trackers-without-cookie";
const stateReceivedTrackers = "received-trackers";
const stateResponseType = "response-tracker-with-cookie";

function reset()
{
  setState(stateCallback, "");
  setState(stateTrackersWithCookie, "");
  setState(stateTrackersWithoutCookie, "");
  setState(stateReceivedTrackers, "");
  setState(stateResponseType, "");
}

function handleRequest(aRequest, aResponse)
{
  let params = new URLSearchParams(aRequest.queryString);

  // init the server and tell the server the total number requests to process
  // server set the cookie
  if (params.has("init")) {
    setState(stateTotalRequests, params.get("init"));

    aResponse.setStatusLine(aRequest.httpVersion, 200);
    aResponse.setHeader("Content-Type", "text/plain", false);

    // Prepare the cookie
    aResponse.setHeader("Set-Cookie", "cookie=1234");
    aResponse.setHeader("Access-Control-Allow-Origin", aRequest.getHeader("Origin"), false);
    aResponse.setHeader("Access-Control-Allow-Credentials","true", false);
    aResponse.write("begin-test");
  // register the callback response, the response will be fired after receiving
  // all the request
  } else if (params.has("callback")) {
    aResponse.processAsync();
    aResponse.setHeader("Content-Type", "text/plain", false);
    aResponse.setHeader("Access-Control-Allow-Origin", aRequest.getHeader("Origin"), false);
    aResponse.setHeader("Access-Control-Allow-Credentials","true", false);

    setState(stateResponseType, params.get("callback"));
    setObjectState(stateCallback, aResponse);
  } else {
    let count = parseInt(getState(stateReceivedTrackers) || 0) + 1;
    setState(stateReceivedTrackers, count.toString());

    let state = "";
    if (aRequest.hasHeader("Cookie")) {
      state = stateTrackersWithCookie;
    } else {
      state = stateTrackersWithoutCookie;
    }

    let ids = params.get("id").concat(",", getState(state));
    setState(state, ids);

    if (getState(stateTotalRequests) == getState(stateReceivedTrackers)) {
      getObjectState(stateCallback, r => {
        if (getState(stateResponseType) == "with-cookie") {
          r.write(getState(stateTrackersWithCookie));
        } else {
          r.write(getState(stateTrackersWithoutCookie));
        }
        r.finish();
        reset();
      });
    }
  }
}
