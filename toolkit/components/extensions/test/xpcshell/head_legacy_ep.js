"use strict";

// Bug 1646182: Test the legacy ExtensionPermission backend until we fully
// migrate to rkv

{
  const { ExtensionPermissions } = ChromeUtils.import(
    "resource://gre/modules/ExtensionPermissions.jsm"
  );

  ExtensionPermissions._useLegacyStorageBackend = true;
  ExtensionPermissions._uninit();
}
