/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cu } = require("chrome");

const { Promise: promise } = require("resource://gre/modules/Promise.jsm");

const { OS } = Cu.import("resource://gre/modules/osfile.jsm", {});
const { TextEncoder, TextDecoder } = Cu.import('resource://gre/modules/commonjs/toolkit/loader.js', {});
const gcli = require("gcli/index");

loader.lazyGetter(this, "prefBranch", function() {
  let prefService = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefService);
  return prefService.getBranch(null).QueryInterface(Ci.nsIPrefBranch2);
});

loader.lazyGetter(this, "supportsString", function() {
  return Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
});

loader.lazyImporter(this, "NetUtil", "resource://gre/modules/NetUtil.jsm");

const PREF_DIR = "devtools.commands.dir";

/**
 * Load all the .mozcmd files in the directory pointed to by PREF_DIR
 * @return A promise of an array of items suitable for gcli.addItems or
 * using in gcli.addItemsByModule
 */
function loadItemsFromMozDir() {
  let dirName = prefBranch.getComplexValue(PREF_DIR,
                                           Ci.nsISupportsString).data.trim();
  if (dirName == "") {
    return promise.resolve([]);
  }

  // replaces ~ with the home directory path in unix and windows
  if (dirName.indexOf("~") == 0) {
    let dirService = Cc["@mozilla.org/file/directory_service;1"]
                      .getService(Ci.nsIProperties);
    let homeDirFile = dirService.get("Home", Ci.nsIFile);
    let homeDir = homeDirFile.path;
    dirName = dirName.substr(1);
    dirName = homeDir + dirName;
  }

  // statPromise resolves to nothing if dirName is a directory, or it
  // rejects with an error message otherwise
  let statPromise = OS.File.stat(dirName);
  statPromise = statPromise.then(
    function onSuccess(stat) {
      if (!stat.isDir) {
        throw new Error("'" + dirName + "' is not a directory.");
      }
    },
    function onFailure(reason) {
      if (reason instanceof OS.File.Error && reason.becauseNoSuchFile) {
        throw new Error("'" + dirName + "' does not exist.");
      } else {
        throw reason;
      }
    }
  );

  // We need to return (a promise of) an array of items from the *.mozcmd
  // files in dirName (which we can assume to be a valid directory now)
  return statPromise.then(() => {
    let itemPromises = [];

    let iterator = new OS.File.DirectoryIterator(dirName);
    let iterPromise = iterator.forEach(entry => {
      if (entry.name.match(/.*\.mozcmd$/) && !entry.isDir) {
        itemPromises.push(loadCommandFile(entry));
      }
    });

    return iterPromise.then(() => {
      iterator.close();
      return promise.all(itemPromises).then((itemsArray) => {
        return itemsArray.reduce((prev, curr) => {
          return prev.concat(curr);
        }, []);
      });
    }, reason => { iterator.close(); throw reason; });
  });
}

exports.mozDirLoader = function(name) {
  return loadItemsFromMozDir().then(items => {
    return { items: items };
  });
};

/**
 * Load the commands from a single file
 * @param OS.File.DirectoryIterator.Entry entry The DirectoryIterator
 * Entry of the file containing the commands that we should read
 */
function loadCommandFile(entry) {
  let readPromise = OS.File.read(entry.path);
  return readPromise = readPromise.then(array => {
    let decoder = new TextDecoder();
    let source = decoder.decode(array);
    var principal = Cc["@mozilla.org/systemprincipal;1"]
                      .createInstance(Ci.nsIPrincipal);

    let sandbox = new Cu.Sandbox(principal, {
      sandboxName: entry.path
    });
    let data = Cu.evalInSandbox(source, sandbox, "1.8", entry.name, 1);

    if (!Array.isArray(data)) {
      console.error("Command file '" + entry.name + "' does not have top level array.");
      return;
    }

    return data;
  });
}

exports.items = [
  {
    name: "cmd",
    get hidden() {
      return !prefBranch.prefHasUserValue(PREF_DIR);
    },
    description: gcli.lookup("cmdDesc")
  },
  {
    name: "cmd refresh",
    description: gcli.lookup("cmdRefreshDesc"),
    get hidden() {
      return !prefBranch.prefHasUserValue(PREF_DIR);
    },
    exec: function(args, context) {
      gcli.load();

      let dirName = prefBranch.getComplexValue(PREF_DIR,
                                              Ci.nsISupportsString).data.trim();
      return gcli.lookupFormat("cmdStatus2", [ dirName ]);
    }
  },
  {
    name: "cmd setdir",
    description: gcli.lookup("cmdSetdirDesc"),
    params: [
      {
        name: "directory",
        description: gcli.lookup("cmdSetdirDirectoryDesc"),
        type: {
          name: "file",
          filetype: "directory",
          existing: "yes"
        },
        defaultValue: null
      }
    ],
    returnType: "string",
    get hidden() {
      return true; // !prefBranch.prefHasUserValue(PREF_DIR);
    },
    exec: function(args, context) {
      supportsString.data = args.directory;
      prefBranch.setComplexValue(PREF_DIR, Ci.nsISupportsString, supportsString);

      gcli.load();

      return gcli.lookupFormat("cmdStatus2", [ args.directory ]);
    }
  }
];
