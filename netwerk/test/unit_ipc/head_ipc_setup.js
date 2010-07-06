// If using NSPR logging, create child log as "${NSPR_LOG_FILE}.child" 
// - TODO: remove when bug 534764 is fixed
var env = Components.classes["@mozilla.org/process/environment;1"]
          .getService(Components.interfaces.nsIEnvironment);
var log = env.get("NSPR_LOG_FILE");
if (log) 
  env.set("NSPR_LOG_FILE", log + ".child");
