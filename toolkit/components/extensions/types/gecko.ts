/**
 * Global Gecko type declarations.
 */

// @ts-ignore
import type { CiClass } from "lib.gecko.xpidl"

declare global {
  // Other misc types.
  type Browser = InstanceType<typeof XULBrowserElement>;
  type bytestring = string;
  type callback = (...args: any[]) => any;
  type ColorArray = number[];
  type integer = number;
  type JSONValue = null | boolean | number | string | JSONValue[] | { [key: string]: JSONValue };

  interface Document {
    createXULElement(name: string): Element;
    documentReadyForIdle: Promise<void>;
  }
  interface EventTarget {
    ownerGlobal: Window;
  }
  interface Error {
    code;
  }
  interface ErrorConstructor {
    new (message?: string, options?: ErrorOptions, lineNo?: number): Error;
  }
  interface Window {
    gBrowser;
  }
  // HACK to get the static isInstance for DOMException and Window?
  interface Object {
    isInstance(object: any): boolean;
  }

  // XPIDL additions/overrides.

  interface nsISupports {
    // OMG it works!
    QueryInterface?<T extends CiClass<nsISupports>>(aCiClass: T): T['prototype'];
    wrappedJSObject?: object;
  }
  interface nsIProperties {
    get<T extends CiClass<nsISupports>>(prop: string, aCiClass: T): T['prototype'];
  }
  interface nsIPrefBranch {
    getComplexValue<T extends CiClass<nsISupports>>(aPrefName: string, aCiClass: T): T['prototype'];
  }
  // TODO: incorporate above into lib.xpidl.d.ts generation, somehow?

  type Sandbox = typeof globalThis;
  interface nsIXPCComponents_utils_Sandbox {
    (principal: nsIPrincipal | nsIPrincipal[], options: object): Sandbox;
  }
  interface nsIXPCComponents_Utils {
    cloneInto<T>(obj: T, ...args: any[]): T;
    createObjectIn<T>(Sandbox, options?: T): T;
    exportFunction<T extends callback>(f: T, ...args: any[]): T;
    getWeakReference<T extends object>(T): { get(): T };
    readonly Sandbox: nsIXPCComponents_utils_Sandbox;
    waiveXrays<T>(obj: T): T;
  }
  interface nsIDOMWindow extends Window {
    docShell: nsIDocShell;
  }
  interface Document {
    documentURIObject: nsIURI;
    createXULElement(name: string): Element;
  }

  // nsDocShell is the only thing implementing nsIDocShell, but it also
  // implements nsIWebNavigation, and a few others, so this is "ok".
  interface nsIDocShell extends nsIWebNavigation {}
  interface nsISimpleEnumerator extends Iterable<any> {}

  namespace Components {
    type Exception = Error;
  }
  namespace UrlbarUtils {
    type RESULT_TYPE = any;
    type RESULT_SOURCE = any;
  }

  // Various mozilla globals.
  var Cc, Cr, ChromeUtils, Components, dump, uneval;

  // [ChromeOnly] WebIDL, to be generated.
  var BrowsingContext, ChannelWrapper, ChromeWindow, ChromeWorker,
    ClonedErrorHolder, Glean, InspectorUtils, IOUtils, JSProcessActorChild,
    JSProcessActorParent, JSWindowActor, JSWindowActorChild,
    JSWindowActorParent, L10nRegistry, L10nFileSource, Localization,
    MatchGlob, MatchPattern, MatchPatternSet, PathUtils, PreloadedScript,
    StructuredCloneHolder, TelemetryStopwatch, WindowGlobalChild,
    WebExtensionContentScript, WebExtensionParentActor, WebExtensionPolicy,
    XULBrowserElement, nsIMessageListenerManager;

  interface XULElement extends Element {}

  // nsIServices is not a thing.
  interface nsIServices {
    scriptloader: mozIJSSubScriptLoader;
    locale: mozILocaleService;
    intl: mozIMozIntl;
    storage: mozIStorageService;
    appShell: nsIAppShellService;
    startup: nsIAppStartup;
    blocklist: nsIBlocklistService;
    cache2: nsICacheStorageService;
    catMan: nsICategoryManager;
    clearData: nsIClearDataService;
    clipboard: nsIClipboard;
    console: nsIConsoleService;
    cookieBanners: nsICookieBannerService;
    cookies: nsICookieManager & nsICookieService;
    appinfo: nsICrashReporter & nsIXULAppInfo & nsIXULRuntime;
    DAPTelemetry: nsIDAPTelemetry;
    DOMRequest: nsIDOMRequestService;
    dns: nsIDNSService;
    dirsvc: nsIDirectoryService & nsIProperties;
    droppedLinkHandler: nsIDroppedLinkHandler;
    eTLD: nsIEffectiveTLDService;
    policies: nsIEnterprisePolicies;
    env: nsIEnvironment;
    els: nsIEventListenerService;
    fog: nsIFOG;
    focus: nsIFocusManager;
    io: nsIIOService & nsINetUtil & nsISpeculativeConnect;
    loadContextInfo: nsILoadContextInfoFactory;
    domStorageManager: nsIDOMStorageManager & nsILocalStorageManager;
    logins: nsILoginManager;
    obs: nsIObserverService;
    perms: nsIPermissionManager;
    prefs: nsIPrefBranch & nsIPrefService;
    profiler: nsIProfiler;
    prompt: nsIPromptService;
    sysinfo: nsISystemInfo & nsIPropertyBag2;
    qms: nsIQuotaManagerService;
    rfp: nsIRFPService;
    scriptSecurityManager: nsIScriptSecurityManager;
    search: nsISearchService;
    sessionStorage: nsISessionStorageService;
    strings: nsIStringBundleService;
    telemetry: nsITelemetry;
    textToSubURI: nsITextToSubURI;
    tm: nsIThreadManager;
    uriFixup: nsIURIFixup;
    urlFormatter: nsIURLFormatter;
    uuid: nsIUUIDGenerator;
    vc: nsIVersionComparator;
    wm: nsIWindowMediator;
    ww: nsIWindowWatcher;
    xulStore: nsIXULStore;
    ppmm: any;
    cpmm: any;
    mm: any;
  }

  var Ci: nsIXPCComponents_Interfaces;
  var Cu: nsIXPCComponents_Utils;
  var Services: nsIServices;
}
