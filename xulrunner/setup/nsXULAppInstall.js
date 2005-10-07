/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla XULRunner.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg <benjamin@smedbergs.us>
 *
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Mozilla Foundation <http://www.mozilla.org/>. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const nsIFile             = Components.interfaces.nsIFile;
const nsIINIParser        = Components.interfaces.nsIINIParser;
const nsIINIParserFactory = Components.interfaces.nsIINIParserFactory;
const nsILocalFile        = Components.interfaces.nsILocalFile;
const nsISupports         = Components.interfaces.nsISupports;
const nsIXULAppInstall    = Components.interfaces.nsIXULAppInstall;
const nsIZipEntry         = Components.interfaces.nsIZipEntry;
const nsIZipReader        = Components.interfaces.nsIZipReader;

function getDirectoryKey(aKey) {
  try {
    return Components.classes["@mozilla.org/file/directory_service;1"].
      getService(Components.interfaces.nsIProperties).
      get(aKey, nsIFile);
  }
  catch (e) {
    throw "Couln't get directory service key: " + aKey;
  }
}

function createINIParser(aFile) {
  return Components.manager.
    getClassObjectByContractID("@mozilla.org/xpcom/ini-parser-factory;1",
                               nsIINIParserFactory).
    createINIParser(aFile);
}

function copy_recurse(aSource, aDest) {
  var e = aSource.directoryEntries;

  while (e.hasMoreElements()) {
    var f = e.getNext().QueryInterface(nsIFile);
    var leaf = f.leafName;

    var ddest = aDest.clone();
    ddest.append(leaf);

    if (f.isDirectory()) {
      copy_recurse(f, ddest);
    }
    else {
      if (ddest.exists())
        ddest.remove(false);

      f.copyTo(aDest, leaf);
    }
  }
}

const PR_WRONLY = 0x02;
const PR_CREATE_FILE = 0x08;
const PR_TRUNCATE = 0x20;

function openFileOutputStream(aFile) {
  var s = Components.classes["@mozilla.org/network/file-output-stream;1"].
    createInstance(Components.interfaces.nsIFileOutputStream);
  s.init(aFile, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 0644, 0);
  return s;
}

/**
 * An extractor implements the following prototype:
 * readonly attribute nsIINIPaser iniParser;
 * void copyTo(in nsILocalFile root);
 */

function directoryExtractor(aFile) {
  this.mDirectory = aFile;
}

directoryExtractor.prototype = {
  mINIParser : null,

  get iniParser() {
    if (!this.mINIParser) {
      var iniFile = this.mDirectory.clone();
      iniFile.append("application.ini");

      this.mINIParser = createINIParser(iniFile);
    }
    return this.mINIParser;
  },

  copyTo : function de_copyTo(aDest) {
    // Assume the root already exists
    copy_recurse(this.mDirectory, aDest);
  }
};

function zipExtractor(aFile) {
  this.mZipReader = Components.classes["@mozilla.org/libjar/zip-reader;1"].
    createInstance(nsIZipReader);
  this.mZipReader.init(aFile);
  this.mZipReader.open();
  this.mZipReader.test(null);
}

zipExtractor.prototype = {
  mINIParser : null,

  get iniParser() {
    if (!this.mINIParser) {
      // XXXbsmedberg: this is not very unique, guessing could be a problem
      var f = getDirectoryKey("TmpD");
      f.append("application.ini");
      f.createUnique(nsIFile.NORMAL_FILE_TYPE, 0600);

      try {
        this.mZipReader.extract("application.ini", f);
        this.mINIParser = createINIParser(f);
      }
      catch (e) {
        try {
          f.remove();
        }
        catch (ee) { }

        throw e;
      }
      try {
        f.remove();
      }
      catch (e) { }
    }
    return this.mINIParser;
  },

  copyTo : function ze_CopyTo(aDest) {
    var entries = this.mZipReader.findEntries("*");
    while (entries.hasMoreElements()) {
      var entry = entries.getNext().QueryInterface(nsIZipEntry);

      this._installZipEntry(this.mZipReader, entry, aDest);
    }
  },

  _installZipEntry : function ze_installZipEntry(aZipReader, aZipEntry,
                                                 aDestination) {
    var file = aDestination.clone();

    var path = aZipEntry.name;
    var dirs = path.split(/\//);
    var isDirectory = path.match(/\/$/) != null;

    var end = dirs.length;
    if (!isDirectory)
      --end;

    for (var i = 0; i < end; ++i) {
      file.append(dirs[i]);
      if (!file.exists()) {
        file.create(nsIFile.DIRECTORY_TYPE, 0755);
      }
    }

    if (!isDirectory) {
      file.append(dirs[end]);
      aZipReader.extract(path, file);
    }
  }
};

function createExtractor(aFile) {
  if (aFile.isDirectory())
    return new directoryExtractor(aFile);

  return new zipExtractor(aFile);
}

const AppInstall = {

  /* nsISupports */
  QueryInterface : function ai_QI(iid) {
    if (iid.equals(nsIXULAppInstall) ||
        iid.equals(nsISupports))
      return this;

    throw Components.result.NS_ERROR_NO_INTERFACE;
  },

  /* nsIXULAppInstall */
  installApplication : function ai_IA(aAppFile, aDirectory, aLeafName) {
    var extractor = createExtractor(aAppFile);
    var iniParser = extractor.iniParser;

    var appName = iniParser.getString("App", "Name");

    // vendor is optional
    var vendor;
    try {
      vendor = iniParser.getString("App", "Vendor");
    }
    catch (e) { }

    if (aDirectory == null) {
#ifdef XP_WIN
      aDirectory = getDirectoryKey("ProgF");
      if (vendor)
        aDirectory.append(vendor);
#else
#ifdef XP_MACOSX
      aDirectory = getDirectoryKey("LocApp");
      if (vendor)
        aDirectory.append(vendor);
#else
      aDirectory = Components.classes["@mozilla.org/file/local;1"].
        createInstance(nsILocalFile);
      aDirectory.initWithPath("/usr/lib");
      aDirectory.append(vendor.toLowerCase());
#endif
#endif
    }
    else {
      aDirectory = aDirectory.clone();
    }

    if (!aDirectory.exists()) {
      aDirectory.create(nsIFile.DIRECTORY_TYPE, 0755);
    }

    if (aLeafName == "") {
#ifdef XP_MACOSX
      aLeafName = appName + ".app";
#else
#ifdef XP_WIN
      aLeafName = appName;
#else
      aLeafName = appName.toLowerCase();
#endif
#endif
    }

    aDirectory.append(aLeafName);
    if (!aDirectory.exists()) {
      aDirectory.create(nsIFile.DIRECTORY_TYPE, 0755);
    }

#ifdef XP_MACOSX
    aDirectory.append("Contents");
    if (!aDirectory.exists()) {
      aDirectory.create(nsIFile.DIRECTORY_TYPE, 0755);
    }
 
    var version = iniParser.getString("App", "Version");
    var buildID = iniParser.getString("App", "BuildID");

    var infoString = "";
    if (vendor) {
      infoString = vendor + " ";
    }
    infoString += appName + " " + version;

    var plistFile = aDirectory.clone();
    plistFile.append("Info.plist");
    var ostream = openFileOutputStream(plistFile);

    var contents =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" +
    "<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n" +
    "<plist version=\"1.0\">\n" +
    "<dict>\n" +
    "<key>CFBundleInfoDictionaryVersion</key>\n" +
    "<string>6.0</string>\n" +
    "<key>CFBundlePackageType</key>\n" +
    "<string>APPL</string>\n" +
    "<key>CFBundleExecutable</key>\n" +
    "<string>xulrunner</string>\n" +
    "<key>NSAppleScriptEnabled</key>\n" +
    "<true/>\n" +
    "<key>CFBundleGetInfoString</key>\n" +
    "<string>" + infoString + "</string>\n" +
    "<key>CFBundleName</key>\n" +
    "<string>" + appName + "</string>\n" +
    "<key>CFBundleShortVersionString</key>\n" +
    "<string>" + version + "</string>\n" +
    "<key>CFBundleVersion</key>\n" +
    "<string>" + version + "." + buildID + "</string>\n" +
    "</dict>\n" +
    "</plist>";

    // "<key>CFBundleIdentifier</key>\n" +
    // "<string>org.%s.%s</string>\n" +
    // "<key>CFBundleSignature</key>\n" +
    // "<string>MOZB</string>\n" +
    // "<key>CFBundleIconFile</key>\n" +
    // "<string>document.icns</string>\n" +

    ostream.write(contents, contents.length);
    ostream.close();

    var contentsDir = aDirectory.clone();
    contentsDir.append("MacOS");

    var xulrunnerBinary = getDirectoryKey("XCurProcD");
    xulrunnerBinary.append("xulrunner");

    xulrunnerBinary.copyTo(contentsDir, "xulrunner");

    aDirectory.append("Resources");
    extractor.copyTo(aDirectory);
#else
    extractor.copyTo(aDirectory);
    // Need to copy the XULRunner stub (which doesn't exist yet)
#endif
  }
};

const AppInstallFactory = {
  /* nsISupports */
  QueryInterface : function aif_QI(iid) {
    if (iid.equals(Components.interfaces.nsIFactory) ||
        iid.equals(nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  /* nsIFactory */
  createInstance : function aif_CI(aOuter, aIID) {
    if (aOuter)
      throw Components.results.NS_ERROR_NO_AGGREGATION;

    return AppInstall.QueryInterface(aIID);
  },

  lockFactory : function aif_lock(aLock) { }
};

const AppInstallContractID = "@mozilla.org/xulrunner/app-install-service;1";
const AppInstallCID = Components.ID("{00790a19-27e2-4d9a-bef0-244080feabfd}");

const AppInstallModule = {
  /* nsISupports */
  QueryInterface : function mod_QI(iid) {
    if (iid.equals(Components.interfaces.nsIModule) ||
        iid.equals(nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  /* nsIModule */
  getClassObject : function mod_gco(aCompMgr, aClass, aIID) {
    if (aClass.equals(AppInstallCID))
      return AppInstallFactory.QueryInterface(aIID);

    return Components.results.NS_ERROR_FACTORY_NOT_REGISTERED;
  },

  registerSelf : function mod_regself(aCompMgr, aLocation,
                                      aLoaderStr, aType) {
    var reg = aCompMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
    reg.registerFactoryLocation(AppInstallCID,
                                "nsXULAppInstall",
                                AppInstallContractID,
                                aLocation,
                                aLoaderStr,
                                aType);
  },

  unregisterSelf : function mod_unreg(aCompMgr, aLocation, aLoaderStr) {
    var reg = aCompMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
    reg.unregisterFactoryLocation(AppInstallCID,
                                  aLocation);
  },

  canUnload : function mod_unload(aCompMgr) {
    return true;
  }
};

function NSGetModule(compMgr, fileSpec) {
  return AppInstallModule;
}
