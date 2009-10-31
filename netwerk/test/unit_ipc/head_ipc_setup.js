
/** 
 * Turn on e10s IPC-based HTTP for tests in this directory
 * - Someday this will not be needed, once IPC HTTP is the default.
 */
Components.classes["@mozilla.org/process/environment;1"]
          .getService(Components.interfaces.nsIEnvironment)
          .set("NECKO_E10S_HTTP", "1");

