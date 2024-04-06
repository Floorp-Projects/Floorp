/**
 * Types specific to toolkit/extensions code.
 */

// This has every possible property we import from all modules, which is not
// great, but should be manageable and easy to generate for each component.
// ESLint warns if we use one which is not actually defined, so still safe.
type LazyAll = {
  BroadcastConduit: typeof import("ConduitsParent.sys.mjs").BroadcastConduit,
  Extension: typeof import("Extension.sys.mjs").Extension,
  ExtensionActivityLog: typeof import("ExtensionActivityLog.sys.mjs").ExtensionActivityLog,
  ExtensionChild: typeof import("ExtensionChild.sys.mjs").ExtensionChild,
  ExtensionCommon: typeof import("ExtensionCommon.sys.mjs").ExtensionCommon,
  ExtensionContent: typeof import("ExtensionContent.sys.mjs").ExtensionContent,
  ExtensionDNR: typeof import("ExtensionDNR.sys.mjs").ExtensionDNR,
  ExtensionDNRLimits: typeof import("ExtensionDNRLimits.sys.mjs").ExtensionDNRLimits,
  ExtensionDNRStore: typeof import("ExtensionDNRStore.sys.mjs").ExtensionDNRStore,
  ExtensionData: typeof import("Extension.sys.mjs").ExtensionData,
  ExtensionPageChild: typeof import("ExtensionPageChild.sys.mjs").ExtensionPageChild,
  ExtensionParent: typeof import("ExtensionParent.sys.mjs").ExtensionParent,
  ExtensionPermissions: typeof import("ExtensionPermissions.sys.mjs").ExtensionPermissions,
  ExtensionStorage: typeof import("ExtensionStorage.sys.mjs").ExtensionStorage,
  ExtensionStorageIDB: typeof import("ExtensionStorageIDB.sys.mjs").ExtensionStorageIDB,
  ExtensionTelemetry: typeof import("ExtensionTelemetry.sys.mjs").ExtensionTelemetry,
  ExtensionTestCommon: typeof import("resource://testing-common/ExtensionTestCommon.sys.mjs").ExtensionTestCommon,
  ExtensionUtils: typeof import("ExtensionUtils.sys.mjs").ExtensionUtils,
  ExtensionWorkerChild: typeof import("ExtensionWorkerChild.sys.mjs").ExtensionWorkerChild,
  GeckoViewConnection: typeof import("resource://gre/modules/GeckoViewWebExtension.sys.mjs").GeckoViewConnection,
  JSONFile: typeof import("resource://gre/modules/JSONFile.sys.mjs").JSONFile,
  Management: typeof import("Extension.sys.mjs").Management,
  MessageManagerProxy: typeof import("MessageManagerProxy.sys.mjs").MessageManagerProxy,
  NativeApp: typeof import("NativeMessaging.sys.mjs").NativeApp,
  NativeManifests: typeof import("NativeManifests.sys.mjs").NativeManifests,
  PERMISSION_L10N: typeof import("ExtensionPermissionMessages.sys.mjs").PERMISSION_L10N,
  QuarantinedDomains: typeof import("ExtensionPermissions.sys.mjs").QuarantinedDomains,
  SchemaRoot: typeof import("Schemas.sys.mjs").SchemaRoot,
  Schemas: typeof import("Schemas.sys.mjs").Schemas,
  WebNavigationFrames: typeof import("WebNavigationFrames.sys.mjs").WebNavigationFrames,
  WebRequest: typeof import("webrequest/WebRequest.sys.mjs").WebRequest,
  extensionStorageSync: typeof import("ExtensionStorageSync.sys.mjs").extensionStorageSync,
  getErrorNameForTelemetry: typeof import("ExtensionTelemetry.sys.mjs").getErrorNameForTelemetry,
  getTrimmedString: typeof import("ExtensionTelemetry.sys.mjs").getTrimmedString,
};

declare global {
  type Lazy = Partial<LazyAll> & { [k: string]: any };

  type BaseContext = import("ExtensionCommon.sys.mjs").BaseContext;
  type ExtensionChild = import("ExtensionChild.sys.mjs").ExtensionChild;
  type Extension = import("Extension.sys.mjs").Extension;
  type callback = (...any) => any;

  interface nsIDOMProcessChild  {
    getActor(name: "ProcessConduits"): ProcessConduitsChild;
  }

  interface WebExtensionContentScript {
    userScriptOptions: { scriptMetadata: object };
  }

  interface WebExtensionPolicy {
    extension: Extension;
    debugName: string;
    instanceId: string;
    optionalPermissions: string[];
  }

  // Can't define a const generic parameter in jsdocs yet.
  // https://github.com/microsoft/TypeScript/issues/56634
  function ConduitGen<const Send>(_, init: Init<Send>, _actor?): Conduit<Send>;
  type Items<A> = A extends ReadonlyArray<infer U extends string> ? U : never;
}

import { PointConduit, ProcessConduitsChild } from "ConduitsChild.sys.mjs";
import { ConduitAddress } from "ConduitsParent.sys.mjs";

type Conduit<Send> = PointConduit & { [s in `send${Items<Send>}`]: callback };
type Init<Send> = ConduitAddress & { send: Send; };
