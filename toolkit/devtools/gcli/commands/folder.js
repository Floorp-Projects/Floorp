/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cu, CC } = require("chrome");
const { Services } = require("resource://gre/modules/Services.jsm");
const gcli = require("gcli/index");
const dirService = Cc["@mozilla.org/file/directory_service;1"]
                      .getService(Ci.nsIProperties);

function showFolder(aPath) {
  let nsLocalFile = CC("@mozilla.org/file/local;1", "nsILocalFile",
                        "initWithPath");

  try {
    let file = new nsLocalFile(aPath);

    if (file.exists()) {
      file.reveal();
      return gcli.lookupFormat("folderOpenDirResult", [aPath]);
    } else {
      return gcli.lookup("folderInvalidPath");
    }
  } catch (e) {
    return gcli.lookup("folderInvalidPath");
  }
}

exports.items = [
  {
    name: "folder",
    description: gcli.lookup("folderDesc")
  },
  {
    name: "folder open",
    description: gcli.lookup("folderOpenDesc"),
    params: [
      {
        name: "path",
        type: { name: "string", allowBlank: true },
        defaultValue: "~",
        description: gcli.lookup("folderOpenDir")
      }
    ],
    returnType: "string",
    exec: function(args, context) {
      let dirName = args.path;

      // replaces ~ with the home directory path in unix and windows
      if (dirName.indexOf("~") == 0) {
        let homeDirFile = dirService.get("Home", Ci.nsIFile);
        let homeDir = homeDirFile.path;
        dirName = dirName.substr(1);
        dirName = homeDir + dirName;
      }

      return showFolder(dirName);
    }
  },
  {
    name: "folder openprofile",
    description: gcli.lookup("folderOpenProfileDesc"),
    returnType: "string",
    exec: function(args, context) {
      // Get the profile directory.
      let currProfD = Services.dirsvc.get("ProfD", Ci.nsIFile);
      let profileDir = currProfD.path;
      return showFolder(profileDir);
    }
  }
];
