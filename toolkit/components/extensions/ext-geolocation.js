"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

// If the user has changed the permission on the addon to something other than
// always allow, then we want to preserve that choice.  We only set the
// permission if it is not set (unknown_action), and we only remove the
// permission on shutdown if it is always allow.

/* eslint-disable mozilla/balanced-listeners */
extensions.on("startup", (type, extension) => {
  if (extension.hasPermission("geolocation") &&
      Services.perms.testPermission(extension.principal.URI, "geo") == Services.perms.UNKNOWN_ACTION) {
    Services.perms.add(extension.principal.URI, "geo",
                       Services.perms.ALLOW_ACTION,
                       Services.perms.EXPIRE_SESSION);
  }
});

extensions.on("shutdown", (type, extension) => {
  if (extension.hasPermission("geolocation") &&
      Services.perms.testPermission(extension.principal.URI, "geo") == Services.perms.ALLOW_ACTION) {
    Services.perms.remove(extension.principal.URI, "geo");
  }
});
