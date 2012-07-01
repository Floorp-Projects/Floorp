/* This Source Code Form is subject to the terms of the Mozilla Public
 *  * License, v. 2.0. If a copy of the MPL was not distributed with this
 *  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);

  let loadedStateKey = "sandbox_content_loaded";
  switch(request.queryString) {
    case "reset": {
      setState(loadedStateKey, "");
      response.write("reset");
      break;
    }
    case "get_loaded": {
      response.setHeader("Content-Type", "text/plain", false);
      let loaded = getState(loadedStateKey);
      if (loaded)
        response.write(loaded);
      else
        response.write("NOTHING");
      break;
    }
    default: {
      let contentType = decodeURIComponent(request.queryString);
      // set the Content-Type equal to the query string
      response.setHeader("Content-Type", contentType, false);
      // If any content is loaded, append it's content type in state
      let loaded = getState(loadedStateKey);
      if (loaded)
        loaded += ",";
      setState(loadedStateKey, loaded + contentType);
      break;
    }
  }
}
