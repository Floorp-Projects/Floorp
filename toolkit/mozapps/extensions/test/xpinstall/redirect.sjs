// Script has two modes based on the query string. If the mode is "setup" then
// parameters from the query string configure the redirection. If the mode is
// "redirect" then a redirect is returned

function handleRequest(request, response)
{
  let parts = request.queryString.split("&");
  let settings = {};

  parts.forEach(function(aString) {
    let [k, v] = aString.split("=");
    settings[k] = v;
  })

  if (settings.mode == "setup") {
    delete settings.mode;

    // Object states must be an nsISupports
    var state = {
      settings: settings,
      QueryInterface: function(aIid) {
        if (aIid.equals(Components.interfaces.nsISupports))
          return settings;
        throw Components.results.NS_ERROR_NO_INTERFACE;
      }
    }
    state.wrappedJSObject = state;

    setObjectState("xpinstall-redirect-settings", state);
    response.setStatusLine(request.httpVersion, 200, "Ok");
    response.setHeader("Content-Type", "text/plain");
    response.write("Setup complete");
  }
  else if (settings.mode == "redirect") {
    getObjectState("xpinstall-redirect-settings", function(aObject) {
      settings = aObject.wrappedJSObject.settings;
    });

    response.setStatusLine(request.httpVersion, 302, "Found");
    for (var name in settings) {
      response.setHeader(name, settings[name]);
    }
    response.write("Done");
  }
}
