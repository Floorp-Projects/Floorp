// SPDX-License-Identifier: MPL-2.0
/// <reference types="jsr:@types/gecko" />
// Wire contracts for self-authored capture/reference fixtures and their reports.
import type { CDPResult } from "./cdp-types.ts";
import type { FrozenCorpusExtension } from "./corpus-types.ts";
import type { ChromeReference } from "./crx-test-types.ts";

export interface CapturePoint {
  x: number;
  y: number;
  html?: string;
  hit?: string;
}
export interface CaptureImage {
  width: number;
  height: number;
  pixel: number[];
  path?: string;
  bytes?: number;
  sha256?: string;
}
export interface CaptureUI {
  url: string;
  title: string;
  text: string;
  controls: { tag: string; id: string; type?: string; text: string }[];
  images: { width: number; height: number; src: string }[];
  canvases: { width: number; height: number; class: string }[];
}
export interface CaptureEvidence {
  id: string;
  name: string;
  version?: string;
  packageSha256?: string;
  primaryOperation: string;
  primaryOperationResult: string;
  steps: string[];
  reason?: string;
  entry?: CaptureUI;
  resultUI?: CaptureUI;
  savedUI?: CaptureUI;
  media?: CaptureImage;
  canvas?: CaptureImage;
  downloadControl?: CapturePoint;
  finalTargets?: { type: string; url: string; title: string }[];
  browserPages?: { url: string; title: string }[];
  authenticationUI?: CaptureUI;
  failureUI?: CaptureUI[];
  error?: string;
}
export interface CaptureReference extends ChromeReference {
  binary: { sha256: string; relativePath: string };
}
export interface CaptureCorpus {
  extensions: (FrozenCorpusExtension & { error?: string })[];
}
export interface CaptureReport {
  stage: string;
  browser: string;
  reference: CaptureReference;
  fixture: string;
  qualified: boolean;
  primaryOperationPassRate: null;
  extensions: CaptureEvidence[];
  output: string;
  version?: CDPResult<"Browser.getVersion">;
  downloadEvents?: { method: string; [key: string]: unknown }[];
}
export interface PowerVisibility {
  power: string;
  tabCapture: string;
  action?: string;
  commands?: string;
  omnibox?: string;
  actionIn?: boolean;
  commandsIn?: boolean;
  omniboxIn?: boolean;
}
export interface PowerContract {
  keys: string[];
  system: string;
  display: string;
  release: string;
  releaseAgain: string;
  requestKeepAwakeinvalid: false | { name: string; message: string };
  requestKeepAwake: false | { name: string; message: string };
  releaseKeepAwakeextra: false | { name: string; message: string };
}
export interface PowerReport {
  reference: CaptureReference;
  output: string;
  passed: boolean;
  version?: CDPResult<"Browser.getVersion">;
  initial?: { url: string; ready: string; power: string; runtime?: string };
  contract?: PowerContract;
  workerWithPower?: PowerVisibility;
  workersWithoutPermission?: (PowerVisibility & { optional: boolean })[];
}
export interface DesktopChooserResult {
  mode: string;
  callbackCalled: boolean;
  id?: number;
  idType?: string;
  idInteger?: boolean;
  streamId?: string;
  options?: { canRequestAudioTrack: boolean };
  error?: string;
  thrown?: string;
}
export interface DesktopReferenceReport {
  passed: boolean;
  output: string;
  reference: CaptureReference;
  scope: string;
  version?: CDPResult<"Browser.getVersion">;
  binarySha256?: string;
  missingTarget?: CDPResult<"Runtime.evaluate">["result"];
  noGesture?: CDPResult<"Runtime.evaluate">["result"];
  all?: CDPResult<"Runtime.evaluate">["result"];
  error?: string;
}
export interface FixtureMessage {
  target?: string;
  value?: unknown;
  auxiliaryDeclarative?: boolean;
  removeMatch?: boolean;
  auxiliaryProbe?: boolean;
  area?: "local" | "session";
  setAccessLevel?: boolean;
}
export interface FixtureMessageReply {
  value?: unknown;
  senderId?: string;
  receiverId?: string;
  storage?: string;
  tabs?: string;
  getBackgroundPage?: string;
}
export interface AuxiliaryContentReport {
  sessionType?: string;
  value?: { contractValue?: string };
  error?: string;
  injected?: string;
}
export interface FixturePort {
  name: string;
  sender?: { id?: string };
  error?: { message?: string };
  onMessage: { addListener(listener: (message: FixtureMessage) => void): void };
  onDisconnect: { addListener(listener: () => void): void };
  postMessage(message: FixtureMessage | FixtureMessageReply): void;
  disconnect(): void;
}
export interface FixtureContext {
  contextId: string;
  documentId?: string;
  contextType: string;
  documentUrl?: string;
  frameId: number;
  tabId: number;
  windowId: number;
}
export interface FixtureTab {
  id: number;
  windowId: number;
  status?: string;
}
export interface DeclarativeInstance {
  instanceType: string;
}
export interface DeclarativeMatcherOptions {
  pageUrl: { hostEquals: string };
  css: string[];
}
export interface DeclarativeIconOptions {
  imageData: Record<number, ImageData>;
}
export interface DeclarativeScriptOptions {
  js: string[];
}
export interface DeclarativeConstructor<Options = never> {
  new (options?: Options): DeclarativeInstance;
  (options?: Options): DeclarativeInstance;
}
export interface DeclarativeRule {
  id: string;
  conditions: DeclarativeInstance[];
  actions: DeclarativeInstance[];
}
export interface DeclarativeRules {
  addRules(
    rules: DeclarativeRule[],
    callback: (rules: DeclarativeRule[]) => void,
  ): void;
  getRules(callback: (rules: DeclarativeRule[]) => void): void;
  removeRules(callback: (rules: DeclarativeRule[]) => void): void;
  removeRules(
    ids: string[],
    callback: (rules: DeclarativeRule[]) => void,
  ): void;
}
export interface FixtureStorageArea {
  get(key: string): Promise<{ contractValue?: string }>;
  set(values: { contractValue: string }): Promise<void>;
  setAccessLevel(
    options: {
      accessLevel: "TRUSTED_CONTEXTS" | "TRUSTED_AND_UNTRUSTED_CONTEXTS";
    },
  ): Promise<void>;
}
export interface FixtureChrome {
  runtime: {
    id: string;
    lastError?: { message?: string };
    getURL(path: string): string;
    getBackgroundPage?: unknown;
    sendMessage(
      message: { target: "background-info" },
    ): Promise<{ hasDocument: boolean; importScripts: string }>;
    sendMessage(message: FixtureMessage): Promise<FixtureMessageReply>;
    sendMessage(
      id: string,
      message: FixtureMessage,
    ): Promise<FixtureMessageReply>;
    connect(id: string, options: { name: string }): FixturePort;
    getContexts(
      filter: {
        contextTypes?: string[];
        contextIds?: string[];
        documentIds?: string[];
      },
    ): Promise<FixtureContext[]>;
    onMessage: {
      addListener(
        listener: (
          message: FixtureMessage,
          sender: { id?: string },
          respond: (
            reply: FixtureMessageReply | AuxiliaryContentReport | {
              hasDocument: boolean;
              importScripts: string;
            },
          ) => void,
        ) => boolean | void,
      ): void;
    };
    onConnect: { addListener(listener: (port: FixturePort) => void): void };
  };
  offscreen: {
    createDocument(
      options: { url: string; reasons: string[]; justification: string },
    ): Promise<void>;
    closeDocument(): Promise<void>;
    hasDocument(): Promise<boolean>;
  };
  tabs: {
    create(options: { url: string; active: boolean }): Promise<FixtureTab>;
    get(id: number): Promise<FixtureTab>;
    remove(id: number): Promise<void>;
    sendMessage(
      id: number,
      message: FixtureMessage,
    ): Promise<AuxiliaryContentReport>;
  };
  storage: { session: FixtureStorageArea; local: FixtureStorageArea };
  action: {
    getUserSettings(): Promise<{ isOnToolbar: boolean }>;
    setBadgeText(options: { text: string }): Promise<void>;
    getBadgeText(options: object): Promise<string>;
    disable(tabId: number): Promise<void>;
    enable(tabId: number): Promise<void>;
    isEnabled(tabId: number): Promise<boolean>;
    openPopup(): Promise<void>;
  };
  notifications: {
    getPermissionLevel(): Promise<string>;
    create(
      id: string,
      options: {
        type: string;
        title: string;
        message: string;
        iconUrl: string;
        requireInteraction: boolean;
        silent: boolean;
        buttons: { title: string }[];
      },
    ): Promise<string>;
    getAll(): Promise<Record<string, boolean>>;
    update(id: string, options: { message: string }): Promise<boolean>;
    clear(id: string): Promise<boolean>;
  };
  declarativeContent: {
    PageStateMatcher: DeclarativeConstructor<DeclarativeMatcherOptions>;
    ShowPageAction: DeclarativeConstructor;
    ShowAction: DeclarativeConstructor;
    SetIcon: DeclarativeConstructor<DeclarativeIconOptions>;
    RequestContentScript: DeclarativeConstructor<DeclarativeScriptOptions>;
    onPageChanged: DeclarativeRules;
  };
  windows: { update(id: number, options: { focused: boolean }): Promise<void> };
}
export interface ComparisonReport {
  passed: boolean;
  runtimeId?: string;
  urlScheme?: string;
  resource?: string;
  selfMessage?: FixtureMessageReply;
  selfPort?: FixtureMessageReply;
  background?: { hasDocument: boolean; importScripts: string };
  contextsBefore?: FixtureContext[];
  offscreenExists?: boolean;
  offscreenContext?: FixtureContext;
  contextIdFilterCount?: number;
  documentIdFilterCount?: number;
  offscreenMessage?: FixtureMessageReply;
  offscreenExistsAfterClose?: boolean;
  contextsAfterClose?: number;
  replacementDocumentIdChanged?: boolean;
  error?: string;
  stack?: string;
}
export interface AuxiliaryReport {
  passed: boolean;
  cases: Record<string, boolean>;
  beforeAccess?: AuxiliaryContentReport;
  afterAccess?: AuxiliaryContentReport;
  afterRevoke?: AuxiliaryContentReport;
  declarativeEnabled?: boolean;
  declarativeUnmatchedEnabled?: boolean;
  error?: string;
  stack?: string;
}
export interface FixtureGlobals {
  chrome: FixtureChrome;
  document: Document;
  importScripts?: (...urls: string[]) => void;
  floorpDebuggerConfig: { url: string };
  floorpReferenceReport?: ComparisonReport | AuxiliaryReport;
}

export interface NativeNamespaceShape {
  inChrome: boolean;
  keys: string[];
  descriptor: {
    configurable: boolean;
    enumerable: boolean;
    writable: boolean;
    valueType: string;
    get: string;
    set: string;
  } | null;
}
export interface WorkerPermissionState {
  generation: string;
  sameChrome: boolean;
  phase: string;
  power: string;
  tabCapture: string;
  level?: Record<string, string>;
  contains: boolean;
  events: {
    event: string;
    permissions: { origins: string[]; permissions: string[] };
  }[];
  chromeKeys: string[];
  powerShape: NativeNamespaceShape;
  tabCaptureShape: NativeNamespaceShape;
  samePower: boolean | null;
  sameTabCapture: boolean | null;
}
export interface RevokedNativeCall {
  name?: string;
  message?: string;
  returns?: string;
  resolved?: unknown;
}
export interface NativeRootContract {
  ordinaryPrototype: boolean;
  tag: string;
  extensible: boolean;
  unknown: string;
  unknownIn: boolean;
  accessor: { value: number; enumerable: boolean };
  accessorDeleted: boolean;
  readOnly: { set: boolean; value: number; writable: boolean };
  symbol: string;
  inherited: number;
  prototypeRestored: boolean;
  deletedMissingPower: boolean;
  deletedMissingTabCapture: boolean;
}
export interface WorkerPermissionReport {
  reference: CaptureReference;
  output: string;
  passed: boolean;
  version?: CDPResult<"Browser.getVersion">;
  workerTargetId?: string;
  rootContract?: NativeRootContract;
  initial?: WorkerPermissionState;
  phaseBeforeConsent?: string;
  nativeConsent?: { focusedWindow: string; pid: number; title: string };
  afterDecision?: WorkerPermissionState;
  granted?: WorkerPermissionState;
  realAPIs?: {
    system: string;
    release: string;
    capturedTabs: unknown[];
    streamId: string;
    level: Record<string, string>;
  };
  removed?: boolean;
  afterRemovalObserved?: WorkerPermissionState;
  revoked?: WorkerPermissionState;
  revokedCalls?: {
    requestKeepAwake: RevokedNativeCall;
    releaseKeepAwake: RevokedNativeCall;
    getCapturedTabs: RevokedNativeCall;
  };
  finalWorkerTargetId?: string;
  sameWorker?: boolean;
  error?: string;
}
