/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cu } = require("chrome");
const Services = require("Services");
const { OS } = require("resource://gre/modules/osfile.jsm");
const { devtools } = require("resource://gre/modules/devtools/Loader.jsm");
const gcli = require("gcli/index");

const BRAND_SHORT_NAME = Cc["@mozilla.org/intl/stringbundle;1"]
                           .getService(Ci.nsIStringBundleService)
                           .createBundle("chrome://branding/locale/brand.properties")
                           .GetStringFromName("brandShortName");

exports.items = [
  {
    name: "tools",
    description: gcli.lookupFormat("toolsDesc2", [ BRAND_SHORT_NAME ]),
    manual: gcli.lookupFormat("toolsManual2", [ BRAND_SHORT_NAME ]),
    get hidden() gcli.hiddenByChromePref(),
  },
  {
    name: "tools srcdir",
    description: gcli.lookup("toolsSrcdirDesc"),
    manual: gcli.lookupFormat("toolsSrcdirManual2", [ BRAND_SHORT_NAME ]),
    get hidden() gcli.hiddenByChromePref(),
    params: [
      {
        name: "srcdir",
        type: "string" /* {
          name: "file",
          filetype: "directory",
          existing: "yes"
        } */,
        description: gcli.lookup("toolsSrcdirDir")
      }
    ],
    returnType: "string",
    exec: function(args, context) {
      let clobber = OS.Path.join(args.srcdir, "CLOBBER");
      return OS.File.exists(clobber).then(function(exists) {
        if (exists) {
          let str = Cc["@mozilla.org/supports-string;1"]
                      .createInstance(Ci.nsISupportsString);
          str.data = args.srcdir;
          Services.prefs.setComplexValue("devtools.loader.srcdir",
                                         Ci.nsISupportsString, str);
          devtools.reload();

          let msg = gcli.lookupFormat("toolsSrcdirReloaded", [ args.srcdir ]);
          throw new Error(msg);
        }

        return gcli.lookupFormat("toolsSrcdirNotFound", [ args.srcdir ]);
      });
    }
  },
  {
    name: "tools builtin",
    description: gcli.lookup("toolsBuiltinDesc"),
    manual: gcli.lookup("toolsBuiltinManual"),
    get hidden() gcli.hiddenByChromePref(),
    returnType: "string",
    exec: function(args, context) {
      Services.prefs.clearUserPref("devtools.loader.srcdir");
      devtools.reload();
      return gcli.lookup("toolsBuiltinReloaded");
    }
  },
  {
    name: "tools reload",
    description: gcli.lookup("toolsReloadDesc"),
    get hidden() {
      return gcli.hiddenByChromePref() ||
             !Services.prefs.prefHasUserValue("devtools.loader.srcdir");
    },

    returnType: "string",
    exec: function(args, context) {
      devtools.reload();
      return gcli.lookup("toolsReloaded2");
    }
  }
];
