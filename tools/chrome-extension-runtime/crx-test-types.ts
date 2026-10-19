// SPDX-License-Identifier: MPL-2.0
import type {
  FrozenCorpusScriptEvidence,
  NativeCorpusBackgroundEvidence,
} from "./native-test-types.ts";
/** Concrete wire, UI and evidence records used by CRX/native-host qualifications. */
export interface ChromeReference {
  browserVersion: string;
  binary: { sha256: string };
}
export interface BrowserVersion {
  product: string;
  protocolVersion?: string;
  userAgent?: string;
}
export interface TargetInfo {
  targetId: string;
  type: string;
  url: string;
  title: string;
}
export interface TargetList {
  targetInfos: TargetInfo[];
}
export interface Evaluation<T = unknown> {
  result: { value: T };
  exceptionDetails?: unknown;
}
export interface MediaProbe {
  streams: { codec_type: string; codec_name: string }[];
  format: { duration: string };
}
export interface SavedMedia {
  path: string;
  bytes: number;
  sha256: string;
  streams?: { type: string; codec: string }[];
  duration?: string | number;
  decodedFirstThreeFramesSha256?: string;
  source?: string;
}
export interface UISnapshot {
  url: string;
  title: string;
  ready?: string;
  readyState?: string;
  text?: string;
  shadowText?: string;
  controls: { tag: string; id: string; type?: string; label: string }[];
}
export interface Playback {
  currentTime: number;
  width: number;
  height: number;
  duration: number;
  error?: string;
}
export interface ClickEvidence {
  text: string;
  html: string;
}
export interface MediaActor {
  sendQuery(name: "Snapshot"): Promise<UISnapshot>;
  sendQuery(name: "Playback"): Promise<Playback | null>;
  sendQuery(name: "Click", data: { text: string }): Promise<ClickEvidence>;
}
export interface NativeBrowser {
  contentTitle: string;
  browsingContext: {
    currentWindowGlobal: { getActor(name: string): MediaActor } | null;
  };
  messageManager: { loadFrameScript(script: string, delayed: boolean): void };
}
export interface NativeTab {
  linkedBrowser: NativeBrowser;
}
export interface BrowserWindow {
  gBrowser: {
    tabs: NativeTab[];
    selectedTab: NativeTab;
    addTab(url: string, options: { triggeringPrincipal: unknown }): NativeTab;
    removeTab(tab: NativeTab, options?: { skipPermitUnload: boolean }): void;
  };
}
export interface InstalledAddon {
  uninstall(): Promise<void>;
}
export interface NativeExtension {
  baseURI: { resolve(path: string): string };
  principal: unknown;
  backgroundContext?: { viewType?: string; xulBrowser: NativeBrowser };
  policy: { backgroundServiceWorkerAllowed: boolean };
  views: Set<{ viewType: string; xulBrowser: NativeBrowser }>;
  apiManager: {
    asyncGetAPI(
      name: string,
      extension: NativeExtension,
      context: string,
    ): Promise<{ triggerAction(window: BrowserWindow): void }>;
  };
}
export interface Download {
  succeeded: boolean;
  target: { path: string };
  source: { url: string };
}
export interface MediaItem {
  id: string;
  name: string;
  version?: string;
  packageSha256?: string;
  primaryOperationAsSelected: string;
  actualAdvertisedOperation: string;
  nativeCompanionApplicability: string;
  nativeCompanionSource: string;
  primaryOperationResult: string;
  snapshots: (UISnapshot & { label: string })[];
  input?: ClickEvidence[];
  unchangedScriptCount?: number;
  unchangedScriptsSha256?: string;
  sourceIntegrity?: FrozenCorpusScriptEvidence;
  nativeEvidence?: NativeCorpusBackgroundEvidence;
  acceptedVendorTermsForAuthoredMedia?: boolean;
  playback?: Playback;
  savedFile?: SavedMedia;
  error?: string;
  additionalErrors?: unknown;
  warnings?: unknown;
  nativeAction?: unknown;
}
export interface MediaReport {
  stage: string;
  browser: string;
  reference?: ChromeReference;
  runtime?: { version: string; buildId: string };
  output: string;
  extension: MediaItem;
  fixture: {
    pageSha256?: string;
    mediaSha256?: string;
    mediaBytes?: number;
    url?: string;
    requests?: { url?: string; range: string | null }[];
  };
  version?: BrowserVersion;
  events?: { method: string }[];
}
export interface InspectionItem {
  id: string;
  name: string;
  version?: string;
  area: string;
  primaryOperation: string;
  primaryOperationResult: string;
  status?: string;
  background?: unknown;
  entries?: {
    popup?: string;
    sidePanel?: string;
    options?: string;
    devtools?: string;
    newtab?: string;
  };
  targets?: Pick<TargetInfo, "type" | "url" | "title">[];
  ui?: Omit<UISnapshot, "controls"> & { controls: unknown[] };
  error?: string;
  uninstallError?: string;
}
export interface InspectionReport {
  stage: string;
  reference: ChromeReference;
  primaryOperationPassRate: null;
  extensions: InspectionItem[];
  output: string;
  version?: BrowserVersion;
}
export interface ContextRecord {
  contextType: string;
  documentId?: string;
  frameId: number;
  tabId: number;
  windowId: number;
}
export interface ComparisonResult {
  passed: boolean;
  error?: string;
  runtimeId: string;
  resource: unknown;
  selfMessage: unknown;
  selfPort: unknown;
  contextsBefore: ContextRecord[];
  offscreenContext: ContextRecord;
  offscreenExists: boolean;
  contextIdFilterCount: number;
  documentIdFilterCount: number;
  offscreenMessage: unknown;
  offscreenExistsAfterClose: boolean;
  contextsAfterClose: unknown[];
  replacementDocumentIdChanged: boolean;
  urlScheme: string;
  background: { hasDocument: boolean };
  transformedBackground?: { service_worker?: string };
}
export interface ComparisonReport {
  passed: boolean;
  fixture: {
    name: string;
    version: string;
    originalId: string;
    sha256: string;
  };
  scope: string;
  output: string;
  reference: ChromeReference;
  lockedRuntimeSourceCommit: string;
  chromeVersion?: BrowserVersion;
  chromeBinarySha256?: string;
  chrome?: ComparisonResult;
  floorp?: ComparisonResult;
  normalized?: { chrome: unknown; floorp: unknown };
  matches?: boolean;
  expectedDifferences?: unknown;
  error?: string;
  stack?: string;
}
export interface NativeCaller {
  id: string;
  manifestVersion: number;
  manifest: Record<string, unknown>;
}
export interface NativeContextInput {
  extension: NativeCaller;
  manifestVersion?: number;
  envType: string;
  url?: null;
  jsonStringify?: typeof JSON.stringify;
  cloneScope?: object;
  logError?(): void;
  preprocessors?: Record<string, unknown>;
}
export interface NativeClient<T = unknown> {
  on(event: "message", listener: (event: string, value: T) => void): void;
  on(
    event: "disconnect",
    listener: (event: string, error?: Error) => void,
  ): void;
  send(message: object): void;
  _cleanup(): Promise<void>;
}
export interface HostManifest {
  name: string;
  description: string;
  path: string;
  type: string;
  allowed_origins?: string[];
  allowed_extensions?: string[];
}
export interface NativeHostInfo {
  id: string;
  version: string;
  binary: string;
  target: string;
  converterBase?: string;
  converterBaseVersion?: string;
}
export interface HostReply {
  _reply: number;
  _result?: unknown;
  _error?: string;
}
export interface PendingRPC {
  resolve(value: unknown): void;
  reject(reason: unknown): void;
  timer: number | ReturnType<typeof import("node:timers").setTimeout>;
}
export interface CompanionOptions {
  media?: string;
  path?: string;
}
export interface CompanionResult {
  id: string;
  version: string;
  binary?: string;
  target?: string;
  runtimeId?: string;
  nativeRequestCount: number;
  bytes?: number;
}
export interface CompanionReply {
  completed: true;
  result?: CompanionResult;
  error?: string;
}
export interface CompanionCase extends CompanionReply {
  label: string;
  addonId: string;
  chromeMode: boolean;
  nativeMessagingPermission: boolean;
  background: string;
}
export interface CompanionReport {
  stage: string;
  frozenCorpusScoreContribution: number;
  caller: string;
  coverage?: string;
  runtime?: { version: string; buildId: string };
  reference?: ChromeReference;
  output?: string;
  passed: boolean;
  cases?: CompanionCase[];
  lookup?: { path: string; chromeOrigin: string; executable: string } | null;
  vendorProvenance?: Record<string, unknown>;
  vendor?: Record<string, unknown>;
  native?: CompanionResult;
  savedFile?: SavedMedia;
  workerSavedFile?: SavedMedia;
  nativeRequestCount?: number;
  error?: string;
}
export interface NativePort {
  onMessage: { addListener(listener: (message: HostReply) => void): void };
  onDisconnect: { addListener(listener: () => void): void };
  postMessage(
    message: {
      type: string;
      _request: number;
      _method: string;
      _args: unknown[];
    },
  ): void;
  disconnect(): void;
}
export interface ChromeNativeAPI {
  runtime: {
    id: string;
    lastError?: { message?: string };
    connectNative(application: string): NativePort;
  };
}
