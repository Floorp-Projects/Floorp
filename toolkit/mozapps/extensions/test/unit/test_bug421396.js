/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const TESTID = "bug421396@tests.mozilla.org";
var INSTALLED = true;

// This creates a fake directory that cannot be modified. Nothing exists inside
// the directory.
function RestrictedPath(source, isVisible) {
  this.source = source;
  this.isVisible = isVisible;
}

// This doesn't implement all of nsIFile and nsILocalFile, just enough to keep
// the EM happy.
RestrictedPath.prototype = {
  // A real nsIFile that this shadows
  source: null,
  // If this file is visible or not. Only the main directory for the addon is visible.
  isVisible: null,

  append: function(node) {
    this.source.append(node);
    this.isVisible = false;
  },

  normalize: function() {
    this.source.normalize();
  },

  create: function(type, permissions) {
    if (this.isVisible)
      throw Components.errors.NS_ERROR_FILE_ALREADY_EXISTS;
    throw Components.errors.NS_ERROR_FILE_ACCESS_DENIED;
  },

  get leafName() {
    return this.source.leafName;
  },

  copyTo: function(parentdir, name) {
    throw Components.errors.NS_ERROR_FILE_ACCESS_DENIED;
  },

  copyToFollowingLinks: function(parentdir, name) {
    throw Components.errors.NS_ERROR_FILE_ACCESS_DENIED;
  },
  
  moveTo: function(parentdir, name) {
    throw Components.errors.NS_ERROR_FILE_ACCESS_DENIED;
  },

  remove: function(recursive) {
    if (this.isVisible)
      throw Components.errors.NS_ERROR_FILE_ACCESS_DENIED;
    throw Components.errors.NS_ERROR_FILE_NOT_FOUND;
  },

  get lastModifiedTime() {
    if (this.isVisible)
      return Date.now();
    throw Components.errors.NS_ERROR_FILE_NOT_FOUND;
  },

  get lastModifiedTimeOfLink() {
    return this.lastModifiedTime;
  },

  get path() { return this.source.path; },
  exists: function() { return this.isVisible; },
  isDirectory: function() { return this.isVisible; },
  isFile: function() { return !this.isVisible; },

  clone: function() {
    return new RestrictedPath(this.source.clone(), this.isVisible);
  },

  get parent() {
    return new RestrictedPath(this.source.parent, false);
  },

  getRelativeDescriptor: function(basedir) {
    return this.source.getRelativeDescriptor(basedir);
  },

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIFile)
     || iid.equals(Components.interfaces.nsILocalFile)
     || iid.equals(Components.interfaces.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};

function DirectoryEnumerator(files) {
  this.files = files;
  this.pos = 0;
}

DirectoryEnumerator.prototype = {
  pos: null,
  files: null,

  get nextFile() {
    if (this.pos < this.files.length)
      return this.files[this.pos++];
    return null;
  },
  
  close: function() {
  }
};

var InstallLocation = {
  name: "test-location",
  location: null,
  restricted: null,
  canAccess: null,
  priority: Components.interfaces.nsIInstallLocation.PRIORITY_APP_SYSTEM_GLOBAL,

  get itemLocations() {
    return new DirectoryEnumerator([ this.getItemLocation(TESTID) ]);
  },

  getItemLocation: function(id) {
    if (id == TESTID) {
      var file = do_get_file("data/test_bug421396");
      if (INSTALLED)
        return file;
      // If we are simulating after the extension is "removed" then return the
      // empty undeletable directory.
      return new RestrictedPath(file, true);
    }
    var file = do_get_cwd();
    file.append("INVALIDNAME");
    file.append(id);
    return file;
  },

  getIDForLocation: function(file) {
    if (file.leafName == "test_bug421396")
      return TESTID;
    return file.leafName;
  },

  getItemFile: function(id, filePath) {
    var itemLocation = this.getItemLocation(id).clone();
    var parts = filePath.split("/");
    for (let part of parts)
      itemLocation.append(part);
    return itemLocation;
  },

  itemIsManagedIndependently: function(id) {
    return false;
  },

  stageFile: function(file, id) {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },

  getStageFile: function(id) {
    return null;
  },

  removeFile: function(file) {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIInstallLocation)
     || iid.equals(Components.interfaces.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
}

var InstallLocationFactory = {
  createInstance: function (outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return InstallLocation.QueryInterface(iid);
  }
};

const IL_CID = Components.ID("{6bdd8320-57c7-4f19-81ad-c32fdfc2b423}");
const IL_CONTRACT = "@tests.mozilla.org/installlocation;1";
var registrar = Components.manager.QueryInterface(Components.interfaces.nsIComponentRegistrar);
registrar.registerFactory(IL_CID, "Test Install Location",
                          IL_CONTRACT, InstallLocationFactory);

var categoryManager = Components.classes["@mozilla.org/categorymanager;1"]
                                .getService(Components.interfaces.nsICategoryManager);
categoryManager.addCategoryEntry("extension-install-locations", "test-location",
                                 IL_CONTRACT, false, true);

function run_test()
{
  // EM needs to be running.
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");
  startupEM();
  // Addon in the fake location should be installed now.
  do_check_neq(gEM.getItemForID(TESTID), null);

  // Mark the add-on as uninstalled and restart ot pickup the change.
  INSTALLED = false;
  restartEM();

  do_check_eq(gEM.getItemForID(TESTID), null);
  // Test install something.
  gEM.installItemFromFile(do_get_addon("test_bug397778"), NS_INSTALL_LOCATION_APPPROFILE);
  do_check_neq(gEM.getItemForID("bug397778@tests.mozilla.org"), null);
  restartEM();

  do_check_eq(gEM.getItemForID(TESTID), null);
  do_check_neq(gEM.getItemForID("bug397778@tests.mozilla.org"), null);
}

