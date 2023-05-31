"use strict";

// Bug 1646182: Test the legacy ExtensionPermission backend until we fully
// migrate to rkv

{
  const { ExtensionPermissions } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionPermissions.sys.mjs"
  );

  ExtensionPermissions._useLegacyStorageBackend = true;
  ExtensionPermissions._uninit();
}
