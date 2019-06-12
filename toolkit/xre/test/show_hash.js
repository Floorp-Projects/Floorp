const xre = Cc["@mozilla.org/xre/directory-provider;1"].getService(Ci.nsIXREDirProvider);
dump(`${xre.getInstallHash(false)}\n`);
