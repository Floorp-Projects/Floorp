/**
 * Shows a message on the page.
 *
 * @param str
 *        The message to show.
 */
function msg(str) {
  document.getElementById("msg").textContent = str;
}

/**
 * Installs the specified webapp and navigates to it in the iframe.
 *
 * @param manifestPath
 *        The path of the manifest relative to
 *        http://mochi.test:8888/tests/webapprt/test/content/.
 * @param parameters
 *        The value to pass as the "parameters" argument to
 *        mozIDOMApplicationRegistry.install, e.g., { receipts: ... }.  Use
 *        undefined to pass nothing.
 */
function installWebapp(manifestPath, parameters) {
  var manifestURL = "http://mochi.test:8888/tests/webapprt/test/content/" +
                    manifestPath;
  var installArgs = [manifestURL, parameters];
  msg("Installing webapp with arguments " + installArgs.toSource() + "...");
  var install = navigator.mozApps.install.apply(navigator.mozApps, installArgs);
  install.onsuccess = function (event) {
    msg("Webapp installed.");
    var testAppURL = install.result.origin +
                     install.result.manifest.launch_path +
                     window.location.search;
    document.getElementById("webapp-iframe").src = testAppURL;
  };
  install.onerror = function () {
    msg("Webapp installation failed with " + install.error.name +
        " for manifest " + manifestURL);
  };
}

/**
 * If webapprt_foo.html is loaded in the window, this function installs the
 * webapp whose manifest is named foo.webapp.
 *
 * @param parameters
 *        The value to pass as the "parameters" argument to
 *        mozIDOMApplicationRegistry.install, e.g., { receipts: ... }.  Use
 *        undefined to pass nothing.
 */
function installOwnWebapp(parameters) {
  var match = /webapprt_(.+)\.html$/.exec(window.location.pathname);
  if (!match) {
    throw new Error("Test URL is unconventional, so could not derive a " +
                    "manifest URL from it: " + window.location);
  }
  installWebapp(match[1] + ".webapp", parameters);
}
