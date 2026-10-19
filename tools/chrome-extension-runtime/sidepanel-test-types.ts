// SPDX-License-Identifier: MPL-2.0
/// <reference types="jsr:@types/gecko" />
// Narrow contracts for immutable corpus fixtures and private native test actors.
export interface ProbeControl extends HTMLElement {
  type?: string;
  value?: string;
}
export interface ProbeMessage {
  name: string;
  data?: { selector?: string; text?: string; value?: string };
}
export interface SidePanelSnapshot {
  url: string;
  title: string;
  ready: string;
  text: string;
  shadowText?: string;
  controls: {
    tag: string;
    id: string;
    type?: string;
    editable?: boolean;
    role?: string | null;
    label: string;
    value?: string;
    html?: string;
  }[];
  viewport?: { width: number; height: number };
}
export interface SidePanelRecipe {
  id?: string;
  readSelector?: string;
  draftSelector?: string;
  submitForRead?: boolean;
  readAfterDraft?: boolean;
  guestReadText?: string;
  guestDismissText?: string[];
  reopenWhenEditorMissing?: string;
  onboardingOrigin?: string;
  refreshText?: string;
  dismissSelectors?: string[];
  guestReadSelector?: string;
  guestSteps?: { selector: string; waitMs?: number; description?: string }[];
  [key: string]: unknown;
}
export interface ReadEvidence {
  domContainsAuthoredText: boolean;
  storageContainsAuthoredText?: boolean;
  nativeHeapContainsAuthoredText?: boolean;
  [key: string]: unknown;
}
export interface SidePanelEvidence {
  id: string;
  name: string;
  version?: string;
  packageSha256?: string;
  primaryOperation?: string;
  result: string;
  checks: string[];
  initialUI?: SidePanelSnapshot;
  afterReadUI?: SidePanelSnapshot;
  readAttemptUI?: SidePanelSnapshot;
  finalUI?: SidePanelSnapshot;
  readEvidence?: ReadEvidence;
  draftBefore?: string | null;
  draftAfter?: string | null;
  draftRetained?: boolean;
  guestSteps?: Record<string, unknown>[];
  otherExtensionViews?: SidePanelSnapshot[];
  targets?: { type: string; url: string; title?: string }[];
  [key: string]: unknown;
}
export interface SidePanelReport {
  stage: string;
  extensions: SidePanelEvidence[];
  output: string;
  [key: string]: unknown;
}
export interface ProbeCommand {
  op: string;
  selector?: string;
  text?: string;
  valueFrom?: string;
  optional?: boolean;
  waitMs?: number;
  value?: string;
}
export interface VaultInfo {
  url: string;
  image: string;
  version?: string;
  accountCreatedThrough?: string;
  accountCreationProof?: string;
}
export interface VaultCredentials {
  email: string;
  password: string;
  username: string;
  masterPassword: string;
  authoredURL?: string;
  vaultURL?: string;
  [key: string]: string | undefined;
}
export interface VaultReport {
  passed: boolean;
  steps: ProbeCommand[];
  [key: string]: unknown;
}
export interface Point {
  x: number;
  y: number;
  width?: number;
  height?: number;
  viewportWidth?: number;
  viewportHeight?: number;
}
export interface PopupPoint extends Point {
  viewportWidth: number;
  viewportHeight: number;
}
export interface FilledValues {
  username?: string;
  password?: string;
  secure?: boolean;
}
export interface ProbeActor {
  sendQuery(name: "Snapshot", data?: unknown): Promise<SidePanelSnapshot>;
  sendQuery(
    name: "ReadDraft" | "TypeDraft",
    data?: unknown,
  ): Promise<{ value: string | null }>;
  sendQuery(name: "ReadFilled", data?: unknown): Promise<FilledValues>;
  sendQuery(name: string, data?: unknown): Promise<unknown>;
}
export interface ProbeBrowser {
  browsingContext: {
    currentWindowGlobal?: {
      getActor(name: string): ProbeActor;
      documentURI: { spec: string };
      innerWindowId: number;
    };
  };
  contentTitle?: string;
}
export interface ProbeTab {
  linkedBrowser: ProbeBrowser;
}
export interface ProbeBrowserWindow extends Window {
  gBrowser: {
    tabs: ProbeTab[];
    selectedTab: ProbeTab;
    addTab(
      url: string,
      options: { triggeringPrincipal: unknown; inBackground?: boolean },
    ): ProbeTab;
    removeTab(tab: ProbeTab, options?: { skipPermitUnload?: boolean }): void;
  };
}
export interface ProbeExtensionView {
  viewType: string;
  unloaded?: boolean;
  xulBrowser?: ProbeBrowser;
}
export interface ProbeExtension {
  views: Set<ProbeExtensionView>;
  principal: nsIPrincipal;
  apiManager: {
    asyncGetAPI(
      name: string,
      extension: ProbeExtension,
      context: string,
    ): Promise<{ triggerAction(window: ProbeBrowserWindow): Promise<void> }>;
  };
}

export interface ReferenceMetadata {
  browserVersion: string;
  binary: { sha256: string };
  [key: string]: unknown;
}
export interface SourceIntegrity {
  scripts: string[];
  [key: string]: unknown;
}
export interface NativeFixtureMetadata {
  pages?: { id?: string; url: string; sha256: string }[];
  [key: string]: unknown;
}
export interface IdentitySnapshot extends Omit<SidePanelSnapshot, "text"> {
  text?: string;
  targetId: string;
  observedAfterMs: number;
  screenshotPath?: string;
}
export interface IdentityObservation {
  id: string;
  name: string;
  version?: string;
  packageSha256?: string;
  primaryOperation: string;
  unchangedScriptCount: number;
  unchangedScriptsSha256: string;
  primaryOperationResult: string;
  prerequisiteStatus: string;
  snapshots: IdentitySnapshot[];
  nativeQualified: false;
  localOnboardingClick?: {
    label: string;
    targetId: string;
    authenticationAttempted: false;
  };
  observationErrors?: string[];
  networkFailures?: { error?: string; status?: number; url?: string }[];
  [key: string]: unknown;
}
export interface IdentityPrerequisiteReport {
  stage: string;
  extensions: IdentityObservation[];
  [key: string]: unknown;
}
