Components.utils.import("resource://gre/modules/Services.jsm");

this.EXPORTED_SYMBOLS = [ "monitor" ];

function notify(event, originalMethod, data, reason) {
  let info = {
    event,
    data: Object.assign({}, data, {
      installPath: data.installPath.path,
      resourceURI: data.resourceURI.spec,
    }),
    reason
  };

  let subject = {wrappedJSObject: {data}};

  Services.obs.notifyObservers(subject, "bootstrapmonitor-event", JSON.stringify(info));

  // If the bootstrap scope already declares a method call it
  if (originalMethod)
    originalMethod(data, reason);
}

// Allows a simple one-line bootstrap script:
// Components.utils.import("resource://xpcshelldata/bootstrapmonitor.jsm").monitor(this);
this.monitor = function(scope, methods = ["install", "startup", "shutdown", "uninstall"]) {
  for (let event of methods) {
    scope[event] = notify.bind(null, event, scope[event]);
  }
}
