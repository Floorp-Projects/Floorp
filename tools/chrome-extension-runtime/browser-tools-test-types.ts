// SPDX-License-Identifier: MPL-2.0
import type { CDPEvent, CDPResult } from "./cdp-types.ts";
import type { FrozenCorpusExtension } from "./corpus-types.ts";
import type { ChromeConversionReport } from "../../browser-features/modules/modules/chrome-web-store/types.ts";

export interface OperationControl {
  tag: string;
  id: string;
  type?: string;
  label?: string;
  text?: string;
}
export interface OperationSnapshot {
  url: string;
  title: string;
  ready: string;
  text: string;
  controls: OperationControl[];
  probe?: string;
  cookie?: string;
}
export interface FocusObservation {
  tag?: string;
  id?: string;
  text?: string;
}
export interface NativeClickObservation {
  clicked: boolean;
  selected: { tag: string; id: string; class: string; pointerEvents: string };
  hit: { tag?: string; class?: string };
  events: { type: string; trusted: boolean; target?: string; id?: string }[];
  x: number;
  y: number;
  viewport: { width: number; height: number };
}
export interface NativeHitObservation {
  selected: { tag: string; class: string; tabIndex: number; html: string };
  hit: { tag?: string; class?: string };
  unobstructed: boolean;
  x: number;
  y: number;
}
export interface NativeKeyObservation {
  pressed: string;
  focusBefore: FocusObservation;
  focusAfter: FocusObservation;
}
export interface OperationReplies {
  Snapshot: OperationSnapshot;
  PressKey: NativeKeyObservation;
  Click: NativeClickObservation;
  InspectHit: NativeHitObservation;
  Type: { value: string };
}
export interface OperationRequest {
  selector?: string;
  text?: string;
  exact?: boolean;
  replace?: boolean;
  key?: string;
}
export interface ActionObservation {
  widgetId: string;
  focused: boolean;
  enabled: unknown;
  disabled: boolean;
  rect: { x: number; y: number; width: number; height: number } | null;
  nativeEvents: { type: string; trusted: boolean }[];
}
export interface BrowserToolsExtensionReport {
  id: string;
  name: string;
  version?: string;
  packageSha256?: string;
  primaryOperation: string;
  primaryOperationResult: string;
  checks: string[];
  snapshots: (OperationSnapshot & { label: string })[];
  unchangedScriptCount?: number;
  unchangedScriptsSha256?: string;
  backgroundSelection?: ChromeConversionReport["background"];
  backgroundContextType?: string | null;
  action?: ActionObservation;
  momentumStage?: string;
  pointerOcclusion?: NativeHitObservation;
  guestPointerOcclusion?: NativeHitObservation;
  guestHit?: NativeHitObservation;
  continueKeyboard?: NativeKeyObservation;
  guestKeyboard?: NativeKeyObservation;
  guestKeyboardFocus?: NativeKeyObservation[];
  continueClick?: NativeClickObservation;
  guestClick?: NativeClickObservation;
  reloadWindows?: { before: number; after: number };
  before?: unknown;
  after?: unknown;
  blocker?: string;
  error?: string;
  errorStack?: string;
  remainingTargets?: CDPResult<"Target.getTargets"> | null;
  restoreCandidates?: unknown;
  consoleEvidence?: CDPEvent[];
}
export interface NativeBrowserToolsReport {
  stage: string;
  output: string;
  runtime: {
    version: string;
    buildId: string;
    workerPolicy: boolean;
    workerWebIDLCompiled?: boolean;
  };
  fixture: {
    pages: { url: string; sha256: string }[];
    entry?: string;
    entrySha256?: string;
    files?: Record<string, string>;
    ordinaryFirefoxControl?: { error?: string; unexpectedlyAllowed?: boolean };
    url?: string;
  };
  extension: BrowserToolsExtensionReport;
}
export interface ReferenceBrowserToolsReport {
  stage: string;
  reference: Record<string, unknown>;
  output: string;
  displayMode?: string;
  version?: CDPResult<"Browser.getVersion">;
  fixture: {
    url: string;
    templateSha256?: string;
    files?: Record<string, string>;
    pageSha256?: string;
    extensionId?: string;
  };
  extension: BrowserToolsExtensionReport;
}
export interface FrozenBrowserToolsCorpus {
  extensions: FrozenCorpusExtension[];
}
