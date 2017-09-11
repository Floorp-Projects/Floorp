/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Delay before responding to an HTTP call attempting to read
// an addon update RDF file

function handleRequest(req, resp) {
  resp.processAsync();
  resp.setHeader("Cache-Control", "no-cache, no-store", false);
  resp.setHeader("Content-Type", "text/xml;charset=utf-8", false);

  let file = null;
  getObjectState("SERVER_ROOT", function(serverRoot)
  {
    file = serverRoot.getFile("browser/toolkit/mozapps/extensions/test/browser/browser_bug557956.rdf");
  });
  dump("*** cancelCompatCheck.sjs: " + file.path + "\n");
  let fstream = Components.classes["@mozilla.org/network/file-input-stream;1"].
                  createInstance(Components.interfaces.nsIFileInputStream);
  fstream.init(file, -1, 0, 0);
  let cstream = null;
  cstream = Components.classes["@mozilla.org/intl/converter-input-stream;1"].
          createInstance(Components.interfaces.nsIConverterInputStream);
  cstream.init(fstream, "UTF-8", 0, 0);

  // The delay can be passed on the query string
  let delay = req.queryString + 0;

  timer = Components.classes["@mozilla.org/timer;1"].
          createInstance(Components.interfaces.nsITimer);
  timer.init(function sendFile() {
    dump("cancelCompatCheck: starting to send file\n");
    let str = {};
    let read = 0;
    do {
      // read as much as we can and put it in str.value
      read = cstream.readString(0xffffffff, str);
      resp.write(str.value);
    } while (read != 0);
    cstream.close();
    resp.finish();
  }, delay, Components.interfaces.nsITimer.TYPE_ONE_SHOT);
}
