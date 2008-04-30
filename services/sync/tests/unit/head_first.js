version(180);

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

let ds = Cc["@mozilla.org/file/directory_service;1"]
  .getService(Ci.nsIProperties);

let provider = {
  getFile: function(prop, persistent) {
    persistent.value = true;
    if (prop == "ExtPrefDL") {
      return [ds.get("CurProcD", Ci.nsIFile)];
    }
    throw Cr.NS_ERROR_FAILURE;
  },
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIDirectoryServiceProvider) ||
        iid.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};
ds.QueryInterface(Ci.nsIDirectoryService).registerProvider(provider);

do_bind_resource(do_get_file("modules"), "weave");
