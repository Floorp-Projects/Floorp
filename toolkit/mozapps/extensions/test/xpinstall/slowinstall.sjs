const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

const RELATIVE_PATH = "browser/toolkit/mozapps/extensions/test/xpinstall"
const NOTIFICATION_TOPIC = "slowinstall-complete";

/**
 * Helper function to create a JS object representing the url parameters from
 * the request's queryString.
 *
 * @param  aQueryString
 *         The request's query string.
 * @return A JS object representing the url parameters from the request's
 *         queryString.
 */
function parseQueryString(aQueryString) {
  var paramArray = aQueryString.split("&");
  var regex = /^([^=]+)=(.*)$/;
  var params = {};
  for (var i = 0, sz = paramArray.length; i < sz; i++) {
    var match = regex.exec(paramArray[i]);
    if (!match)
      throw "Bad parameter in queryString!  '" + paramArray[i] + "'";
    params[decodeURIComponent(match[1])] = decodeURIComponent(match[2]);
  }

  return params;
}

function handleRequest(aRequest, aResponse) {
  let id = +getState("ID");
  setState("ID", "" + (id + 1));

  function LOG(str) {
    dump("slowinstall.sjs[" + id + "]: " + str + "\n");
  }

  aResponse.setStatusLine(aRequest.httpVersion, 200, "OK");

  var params = { };
  if (aRequest.queryString)
    params = parseQueryString(aRequest.queryString);

  if (params.file) {
    let xpiFile = "";

    function complete_download() {
      LOG("Completing download");
      downloadPaused = false;

      try {
        // Doesn't seem to be a sane way to read using OS.File and write to an
        // nsIOutputStream so here we are.
        let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
        file.initWithPath(xpiFile);
        let stream = Cc["@mozilla.org/network/file-input-stream;1"].
                     createInstance(Ci.nsIFileInputStream);
        stream.init(file, -1, -1, stream.DEFER_OPEN + stream.CLOSE_ON_EOF);

        NetUtil.asyncCopy(stream, aResponse.bodyOutputStream, () => {
          LOG("Download complete");
          aResponse.finish();
        });
      }
      catch (e) {
        LOG("Exception " + e);
      }
    }

    let waitForComplete = new Promise(resolve => {
      function complete() {
        Services.obs.removeObserver(complete, NOTIFICATION_TOPIC);
        resolve();
      }

      Services.obs.addObserver(complete, NOTIFICATION_TOPIC, false);
    });

    aResponse.processAsync();

    OS.File.getCurrentDirectory().then(dir => {
      xpiFile = OS.Path.join(dir, ...RELATIVE_PATH.split("/"), params.file);
      LOG("Starting slow download of " + xpiFile);

      OS.File.stat(xpiFile).then(info => {
        aResponse.setHeader("Content-Type", "binary/octet-stream");
        aResponse.setHeader("Content-Length", info.size.toString());

        LOG("Download paused");
        waitForComplete.then(complete_download);
      });
    });
  }
  else if (params.continue) {
    dump("slowinstall.sjs: Received signal to complete all current downloads.\n");
    Services.obs.notifyObservers(null, NOTIFICATION_TOPIC, null);
  }
}
