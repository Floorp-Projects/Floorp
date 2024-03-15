/**
 * Gecko XPCOM builtins and utility types.
 */

/**
 * Generic IDs are created by most code which passes a nsID to js.
 * https://searchfox.org/mozilla-central/source/js/xpconnect/src/XPCJSID.cpp#24
 */
interface nsID<uuid = string> {
  readonly number: uuid;
}

/**
 * In addition to nsID, interface IIDs support instanceof type guards,
 * and expose constants defined on the class, including variants from enums.
 * https://searchfox.org/mozilla-central/source/js/xpconnect/src/XPCJSID.cpp#44
 */
type nsJSIID<iface, enums = {}> = nsID & Constants<iface> & enums & {
  new (_: never): void;
  prototype: iface;
}

/** A union of all known IIDs. */
type nsIID = nsIXPCComponents_Interfaces[keyof nsIXPCComponents_Interfaces];

/** A generic to resolve QueryInterface return type from a nsID (or nsIID). */
export type nsQIResult<iid> = iid extends { prototype: infer U } ? U : never;

/** XPCOM inout param is passed in as a js object with a value property. */
type InOutParam<T> = { value: T };

/** XPCOM out param is written to the passed in object's value property. */
type OutParam<T> = { value?: T };

/** A named type for interfaces to inherit from enums. */
type Enums<enums> = enums;

/** Callable accepts either form of a [function] interface. */
type Callable<iface> = iface | Extract<iface[keyof iface], Function>

/** Picks only const number properties from T. */
type Constants<T> = { [K in keyof T as IfConst<K, T[K]>]: T[K] };

/** Resolves only for keys K whose corresponding type T is a narrow number. */
type IfConst<K, T> = T extends number ? (number extends T ? never : K) : never;

declare global {
  // Until we have [ChromeOnly] webidl.
  interface BrowsingContext {}
  interface ContentFrameMessageManager {}
  interface DOMRequest {}
  interface FrameLoader {}
  interface JSProcessActorChild {}
  interface JSProcessActorParent {}
  interface TreeColumn {}
  interface WebExtensionContentScript {}
  interface WebExtensionPolicy {}
  interface WindowGlobalParent {}
  interface WindowContext {}
  interface XULTreeElement {}
}

// Non-scriptable interfaces referenced from scriptable ones.
interface nsIAsyncVerifyRedirectReadyCallback {}
interface nsICRLiteTimestamp {}
interface nsIInputAvailableCallback {}
interface nsIScriptElement {}
interface nsIThreadObserver {}
interface nsIUDPSocketSyncListener {}
interface nsIWebAuthnRegisterArgs {}
interface nsIWebAuthnRegisterPromise {}
interface nsIWebAuthnSignArgs {}
interface nsIWebAuthnSignPromise {}
interface nsIXPCScriptable {}

// Typedefs useful as a quick reference in method signatures.
type double = number;
type float = number;
type i16 = number;
type i32 = number;
type i64 = number;
type u16 = number;
type u32 = number;
type u64 = number;
type u8 = number;
