// SPDX-License-Identifier: MPL-2.0
/// <reference types="jsr:@types/gecko" />
// Local contracts for authored browser fixtures and their recorded native observations.
import type { Protocol } from "devtools-protocol/types/protocol.d.ts";
import type { CDPEvent } from "./cdp-types.ts";

export type BinaryInput = string | Uint8Array;
export type NativeBinaryInput = ArrayBuffer | Uint8Array;
export interface CorpusRecord {
  id: string;
  name: string;
  area: string;
  primaryOperation: string;
  status: "frozen" | "unavailable";
  version?: string;
  packageSha256?: string;
}
export interface CorpusLock {
  extensions: CorpusRecord[];
}
export interface ChromeReference {
  browserVersion: string;
  binary: { sha256: string };
}
export interface Point {
  x: number;
  y: number;
}
export interface NativeControlPoint extends Point {
  width: number;
  height: number;
}
export interface NativeTabInfo {
  id: string;
  text: string;
  selected: string | null;
  rect: DOMRect;
}
export interface PanelSnapshot {
  url: string;
  ready: string;
  text: string;
}
export interface InputObservation {
  value: string;
  test?: string | null;
  placeholder?: string;
  type?: string;
}
export interface PropsObservation {
  text: string;
  inputs: InputObservation[];
}
export interface DevtoolsSnapshot extends PanelSnapshot {
  reactCount?: string | null;
  reduxCount?: string | null;
  props?: PropsObservation | null;
}
export interface EditorSnapshot {
  url?: string;
  ready?: string;
  text?: string;
  hasEditor?: boolean;
  name?: string;
  code?: string;
  saveDisabled?: boolean;
  controls?: {
    id: string;
    type?: string;
    text: string | null;
    disabled?: boolean;
    checked?: boolean;
    value?: string;
  }[];
}
export interface ContentSnapshot {
  url: string;
  ready: string;
  title?: string;
  body: string | null;
  color: string | null;
  probe: string | null;
  markers: string[];
  text?: string | null;
  userScript?: string;
  frameUserScript?: string;
  authoredScript?: string;
  styles?: { id: string; className: string; text: string }[];
}
export interface MediaSnapshot {
  url: string;
  ready: string;
  visibility: string;
  focused: boolean;
  origin?: number;
  video: {
    readyState: number;
    paused: boolean;
    currentTime: number;
    duration: number;
    playbackRate: number;
    seeking?: boolean;
    seekable?: number[][];
    error?: number;
  } | null;
  bezel?: string | null;
  dislike?: string | null;
  body?: string;
}
export interface FrameworkPackage {
  version: string;
  projectLockKey: string;
  projectLockIntegrity: string;
  cachedFilesSha256: string;
  cachedFileCount: number;
}
export interface FrameworkMetadata {
  source: string;
  packages: Record<string, FrameworkPackage>;
  htmlSha256: string;
  appSha256: string;
}
export interface MediaFixture {
  hosts: string[];
  url: string;
  sourceIntegrity?: Record<string, unknown>;
  [key: string]: unknown;
}
export interface NativePanelDefinition {
  id: string;
  label: string;
  extensionId: string;
}
export interface NativePanelRecord extends NativePanelDefinition {
  url?: string;
}
export interface CorpusOperation<Snapshot = ContentSnapshot> {
  id: string;
  name: string;
  version?: string;
  packageSha256?: string;
  result?: string;
  primaryOperation?: string;
  primaryOperationResult: string;
  nativeQualified?: boolean;
  nativeEvidence?:
    import("./native-test-types.ts").NativeCorpusBackgroundEvidence;
  nativeCandidateIdentity?:
    | import("./native-test-types.ts").NativeCorpusCandidateIdentity
    | null;
  checks: string[];
  baseline?: Snapshot;
  transformed?: Snapshot;
  lastSnapshot?: Snapshot;
  afterNavigation?: Snapshot;
  afterRemoval?: Snapshot;
  beforeNavigation?: Snapshot;
  actualDecodedMedia?: Snapshot;
  played?: Snapshot;
  afterNativeControl?: Snapshot;
  editorBefore?: EditorSnapshot;
  editorAfterTyping?: EditorSnapshot;
  editorSaved?: EditorSnapshot;
  afterNativeSave?: EditorSnapshot;
  userScriptsPermissionBefore?: PermissionControl;
  userScriptsPermissionAfter?: PermissionControl;
  saveControl?: unknown;
  nativeUserscriptRegistration?: unknown;
  authoredUserscriptSha256?: string;
  contextStates?: unknown[];
  contextDiagnosticErrors?: string[];
  nativeExecutionContexts?: unknown[];
  nativeEvents?: CDPEvent[];
  cleanupError?: string;
  error?: string;
  conversion?: unknown;
  warnings?: unknown;
  background?: unknown;
  unchangedScriptCount?: number;
  unchangedScriptsSha256?: string;
  fixtureURL?: string;
  nativeDevtools?: Protocol.Target.TargetInfo;
  nativePanel?: Protocol.Target.TargetInfo | NativePanelRecord;
  selectedTabs?: NativeTabInfo[];
  availableNativeTabs?: NativeTabInfo[];
  nativeTabsDiagnosticError?: string;
  nativePanelDiagnosticError?: string;
  initialPanel?: PanelSnapshot;
  panelFailure?: PanelSnapshot;
  selectedComponent?: PanelSnapshot;
  updatedComponent?: PanelSnapshot;
  propsBefore?: PropsObservation | DevtoolsSnapshot;
  propsAfter?: PropsObservation | DevtoolsSnapshot;
  stateBefore?: PanelSnapshot;
  stateAfter?: PanelSnapshot;
  appBefore?: DevtoolsSnapshot;
  appAfter?: DevtoolsSnapshot;
  registeredPanels?: NativePanelDefinition[];
  onboardingTargets?: Protocol.Target.TargetInfo[];
  lateOnboardingTargets?: Protocol.Target.TargetInfo[];
  sourceIntegrity?: unknown;
  playing?: Snapshot;
  onboardingTabs?: { url: string }[];
  controlBefore?: Snapshot;
  controlAfter?: Snapshot;
  metadata?: unknown;
  nativeTransportRequests?: unknown[];
}
export interface CorpusReport<Snapshot = ContentSnapshot> {
  stage: string;
  nativeQualified?: boolean;
  qualificationMode?: "candidate" | "historical";
  reference?: ChromeReference;
  runtime?: {
    version: string;
    buildId: string;
    nativeWorkerPolicy: boolean;
    workerWebIDLCompiled?: boolean;
  };
  fixture: unknown;
  extensions: CorpusOperation<Snapshot>[];
  output: string;
  primaryOperationPassRate: null;
  version?: Protocol.Browser.GetVersionResponse;
  displayMode?: string;
  events?: CDPEvent[];
}
export interface ActorClickData {
  selector?: string;
  buttonText?: string;
  menuText?: string;
  text?: string;
}
export type ContentUIMessage = { name: "SnapshotUI"; data?: undefined } | {
  name: "Click" | "Type";
  data: ActorClickData;
};
export type DevtoolsMessage = { name: "Snapshot"; data?: undefined } | {
  name: "Click";
  data: { selector: string; text?: string };
};
export type MediaMessage = { name: "Snapshot"; data?: undefined } | {
  name: "Click" | "ControlSpeed";
  data?: { selector?: string };
};
export interface CodeMirrorElement extends HTMLElement {
  CodeMirror?: { getValue(): string };
}
export type ActorHandle = import("./native-test-types.ts").NativeCorpusActor;
export type NativeBrowserHandle =
  import("./native-test-types.ts").NativeCorpusBrowser;
export type NativeTabHandle = import("./native-test-types.ts").NativeCorpusTab;
export type NativeWindowHandle =
  import("./native-test-types.ts").NativeCorpusWindow;
export interface NativeView {
  viewType: string;
  unloaded?: boolean;
  devToolsToolbox?: NativeToolbox;
  xulBrowser?: NativeBrowserHandle;
  uri?: { spec: string };
}
export interface NativeExtensionHandle
  extends
    Omit<import("./native-test-types.ts").NativeCorpusExtension, "views"> {
  views: Set<NativeView>;
}
export interface AssessmentExtensionParent {
  GlobalManager: { extensionMap: Map<string, NativeExtensionHandle> };
}
export interface NativeToolbox {
  currentToolId: string;
  getAdditionalTools(): NativePanelDefinition[];
  selectTool(id: string, reason: string): Promise<unknown>;
  destroy(): Promise<void>;
}
export interface NativeToolboxManager {
  showToolboxForTab(
    tab: NativeTabHandle,
    options: { toolId: string; hostType: string; raise: boolean },
  ): Promise<NativeToolbox | null>;
}
export interface ResourceSubstitutions {
  setSubstitution(name: string, uri: nsIURI | null): void;
}
export interface CounterState {
  count: number;
}
export interface AuthoredAction {
  type: string;
}
export interface ReactCounterProps {
  count: number;
}
export interface ReduxFixtureGlobal {
  __REDUX_DEVTOOLS_EXTENSION__?: (
    options: { name: string },
  ) => import("redux").StoreEnhancer;
}

export type Truthy<T> = Exclude<T, false | null | undefined | 0 | "">;
export interface ChromePanel {
  target: Protocol.Target.TargetInfo;
  sessionId: string;
  evaluate<T>(expression: string): Promise<T>;
  clickExpression(expression: string): Promise<void>;
  snapshot(): Promise<PanelSnapshot>;
}
export interface ChromeFrontend {
  target: Protocol.Target.TargetInfo;
  sessionId: string;
  evaluate<T>(expression: string): Promise<T>;
  tabs(): Promise<NativeTabInfo[]>;
  selectPanel(title: string): Promise<NativeTabInfo[]>;
  attachPanel(extensionId: string, path?: string): Promise<ChromePanel>;
  clickPoint(point: NativeControlPoint): Promise<void>;
}

export type NativeTargetInfo = Protocol.Target.TargetInfo & {
  parentId?: string;
  parentFrameId?: string;
};
export interface BrowserFrameworkGlobal extends ReduxFixtureGlobal {
  document: Document;
}
export interface ProjectNpmLock {
  npm: Record<string, { integrity: string }>;
}
export interface PackageManifest {
  name: string;
  version: string;
}

export type NativeInputWindow = WindowProxy & mozIDOMWindow;

export interface NativeToolboxModule {
  gDevTools: NativeToolboxManager;
}
export interface NativeFloorpFrontend {
  toolbox: NativeToolbox;
  panels(): NativePanelDefinition[];
  selectPanel(
    title: string,
    path?: string,
  ): Promise<
    {
      definition: NativePanelDefinition;
      view: NativeView;
      browser: NativeBrowserHandle;
    }
  >;
  close(): Promise<void>;
}

export interface ContentActorResponses {
  Snapshot: ContentSnapshot;
  SnapshotUI: EditorSnapshot;
  Click: { clicked: boolean };
  Type: unknown;
}
export interface MediaActorResponses {
  Snapshot: MediaSnapshot;
  Click: boolean;
  ControlSpeed: boolean;
}
export interface DevtoolsActorResponses {
  Snapshot: DevtoolsSnapshot;
  Click: boolean;
}
export interface AssessmentExtensionParentModule {
  ExtensionParent: AssessmentExtensionParent;
}
export interface ResourceProtocolHandler {
  QueryInterface(iid: nsIID): ResourceSubstitutions;
}
export type ContextCreatedEvent = CDPEvent & {
  method: "Runtime.executionContextCreated";
  sessionId?: string;
  params: Protocol.Runtime.ExecutionContextCreatedEvent;
};

export interface PermissionControl extends Point {
  checked: boolean;
}

export interface UserScriptRegistrationObservation {
  scripts: unknown[];
  permissions: { permissions: string[]; origins: string[] };
}
export interface ContextDiagnosticSnapshot {
  url: string;
  ready: string;
  init: string;
  vm: string;
  runtimeId?: string;
  scripts: number;
}
