/**
 * Type declarations for WebExtensions framework code.
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

// Utility type to extract all strings from a const array, to use as keys.
type Items<A> = A extends ReadonlyArray<infer U extends string> ? U : never;

declare global {
  type Lazy<Keys extends keyof LazyAll = keyof LazyAll> = Pick<LazyAll, Keys> & { [k: string]: any };

  // Export JSDoc types, and make other classes available globally.
  type ConduitAddress = import("ConduitsParent.sys.mjs").ConduitAddress;
  type ConduitID = import("ConduitsParent.sys.mjs").ConduitID;
  type Extension = import("Extension.sys.mjs").Extension;

  // Something about Class type not being exported when nested in a namespace?
  type BaseContext = InstanceType<typeof import("ExtensionCommon.sys.mjs").ExtensionCommon.BaseContext>;
  type BrowserExtensionContent = InstanceType<typeof import("ExtensionContent.sys.mjs").ExtensionContent.BrowserExtensionContent>;
  type EventEmitter = InstanceType<typeof import("ExtensionCommon.sys.mjs").ExtensionCommon.EventEmitter>;
  type ExtensionAPI = InstanceType<typeof import("ExtensionCommon.sys.mjs").ExtensionCommon.ExtensionAPI>;
  type ExtensionError = InstanceType<typeof import("ExtensionUtils.sys.mjs").ExtensionUtils.ExtensionError>;
  type LocaleData = InstanceType<typeof import("ExtensionCommon.sys.mjs").ExtensionCommon.LocaleData>;
  type ProxyAPIImplementation = InstanceType<typeof import("ExtensionChild.sys.mjs").ExtensionChild.ProxyAPIImplementation>;
  type SchemaAPIInterface = InstanceType<typeof import("ExtensionCommon.sys.mjs").ExtensionCommon.SchemaAPIInterface>;
  type WorkerExtensionError = InstanceType<typeof import("ExtensionUtils.sys.mjs").ExtensionUtils.WorkerExtensionError>;

  // Other misc types.
  type AddonWrapper = any;
  type Context = BaseContext;
  type NativeTab = Element;
  type SavedFrame = object;

  // Can't define a const generic parameter in jsdocs yet.
  // https://github.com/microsoft/TypeScript/issues/56634
  type ConduitInit<Send> = ConduitAddress & { send: Send; };
  type Conduit<Send> = import("../ConduitsChild.sys.mjs").PointConduit & { [s in `send${Items<Send>}`]: callback };
  type ConduitOpen = <const Send>(subject: object, address: ConduitInit<Send>) => Conduit<Send>;
}

export {}
