// SPDX-License-Identifier: MPL-2.0
import type { CDPResult } from "./cdp-types.ts";
import type {
  FrozenCorpusScriptEvidence,
  NativeCorpusBackgroundEvidence,
  NativeCorpusBrowser,
} from "./native-test-types.ts";
export interface VisionMacro {
  Name: string;
  CreationDate: string;
  Commands: { Command: string; Target: string; Value: string }[];
}
export interface WorkflowNode {
  label: string;
  data: Record<string, unknown> & {
    url?: string;
    selector?: string;
    value?: string;
    delay?: string;
  };
}
export interface AutomaWorkflow {
  [property: string]: unknown;
  id: string;
  name: string;
  drawflow: { nodes: WorkflowNode[] };
  isProtected?: boolean;
  isDisabled?: boolean;
  createdAt?: number;
  updatedAt?: number;
}
export interface AutomaStorage {
  workflows?: Record<string, AutomaWorkflow>;
  isFirstTime?: boolean;
  config?: Record<string, unknown>;
  logs?: unknown;
  workflowState?: unknown;
}
export interface Sitemap {
  _id: string;
  startUrl: string[];
  selectors: {
    id: string;
    parentSelectors: string[];
    type: string;
    selector: string;
    multiple: boolean;
    regex?: string;
  }[];
}
export interface AutomationSnapshot {
  url: string;
  ready: string;
  text: string;
  input?: string;
  result?: string | null;
  operation?: string;
  clickCount?: string;
  rows: string[][];
}
export interface AutomationObserved {
  url?: string;
  ready?: string;
  text?: string;
  input?: string;
  result?: string | null;
  operation?: string;
  clickCount?: string;
  rows?: string[][];
}
export interface UIVisionWelcomeObservation {
  shown: boolean;
  clicked: boolean;
  title: string | null;
  events: { type: string; trusted: boolean }[];
}
export type UIVisionWebsiteControl =
  | "websiteRuns"
  | "editWhitelist"
  | "whitelistURL"
  | "saveWhitelist";
export interface UIVisionWebsiteConfiguration {
  allowRunFromHttpSchema?: unknown;
  websiteInvokePolicyVersion?: unknown;
  websiteWhiteList?: unknown;
}
export interface UIVisionWebsiteControlObservation {
  url: string;
  ready: string;
  control?: UIVisionWebsiteControl;
  present: boolean;
  visible: boolean;
  enabled: boolean;
  unobstructed: boolean;
  checked?: boolean;
  value?: string;
  dialogTitle: string | null;
  dialogVisible: boolean;
  events?: { type: string; trusted: boolean }[];
}
export interface UIVisionNavigationBrowser extends NativeCorpusBrowser {
  webProgress: Pick<nsIWebProgress, "isLoadingDocument">;
}
export interface UIVisionDocumentChange {
  error: string;
  beforeInnerWindowId: number;
  currentInnerWindowId: number | null;
  isLoadingDocument: boolean;
  url: string;
}
export type UIVisionLogControl = "data" | "logs";
export interface UIVisionLogControlObservation {
  url: string;
  ready: string;
  control: UIVisionLogControl;
  present: boolean;
  visible: boolean;
  enabled: boolean;
  unobstructed: boolean;
  selected: boolean;
  viewVisible: boolean;
  rect: { x: number; y: number; width: number; height: number };
  events?: { type: string; trusted: boolean }[];
}
export interface UIVisionInvocationObservation {
  dispatchedDocumentedBridge: boolean;
  witnessMs: number;
  events: { type: string; message?: string }[];
  observerToken?: string;
  transport?: "bounded-owned-document-console" | "Runtime.consoleAPICalled";
  documentChanges?: UIVisionDocumentChange[];
}
export interface AutomationItem {
  id: string;
  name: string;
  version?: string;
  packageSha256?: string;
  result: string;
  unchangedScriptCount?: number;
  unchangedScriptsSha256?: string;
  sourceIntegrity?: FrozenCorpusScriptEvidence;
  nativeEvidence?: NativeCorpusBackgroundEvidence;
  warnings?: string[];
  configuration?: unknown;
  onboarding?: UIVisionWelcomeObservation;
  websiteOperator?: {
    before: UIVisionWebsiteConfiguration;
    after?: UIVisionWebsiteConfiguration;
    steps: UIVisionWebsiteControlObservation[];
  };
  invocation?: UIVisionInvocationObservation;
  macro?: VisionMacro;
  workflow?: AutomaWorkflow;
  observed?: AutomationObserved | null;
  engineLog?: string;
  engineView?: unknown;
  engineState?: AutomaStorage;
  initialUI?: string;
  workerTarget?: CDPResult<"Target.getTargets">["targetInfos"][number];
  executeControl?: { x: number; y: number };
  execute?: unknown;
  actualActionUI?: { url: string; text: string; buttons: string[] };
  checks?: string[];
  reason?: string;
  blocker?: string;
  error?: string;
  nativeDevtools?: unknown;
  selectedTabs?: unknown;
  nativePanel?: unknown;
  sitemap?: Sitemap;
  importedControlState?: ScraperControlState;
  scrapeMenuControlState?: ScraperControlState;
  input?: unknown;
  imported?: { url: string } | null;
  rows?: string[][];
  nativeAction?: unknown;
}
export interface AutomationReport {
  stage: string;
  runtime?: { version: string; buildId: string };
  reference?: unknown;
  fixture?: unknown;
  extensions: AutomationItem[];
  output: string;
}
export type ScraperControl =
  | "createMenu"
  | "importForm"
  | "sitemapJSON"
  | "import"
  | "sitemapMenu"
  | "scrapeForm"
  | "start"
  | "refresh";
export interface ScraperControlState {
  url: string;
  ready: string;
  control: ScraperControl;
  present: boolean;
  visible: boolean;
  enabled: boolean;
  unobstructed: boolean;
  activeSitemapId: string;
  selectorViewReady: boolean;
}
export interface AutomationActorData {
  control?: ScraperControl;
  sitemap?: Sitemap;
  url?: string;
  macro?: VisionMacro;
  workflow?: AutomaWorkflow;
  name?: string;
  uiVisionControl?: UIVisionWebsiteControl;
  uiVisionLogControl?: UIVisionLogControl;
  witnessToken?: string;
}
export interface AutomationActor {
  sendQuery(name: "Snapshot"): Promise<AutomationSnapshot>;
  sendQuery(
    name: "AcknowledgeUIVisionWelcome",
  ): Promise<UIVisionWelcomeObservation>;
  sendQuery(
    name: "ReadAutomaConfiguration" | "ReadAutomaEngineState",
  ): Promise<AutomaStorage>;
  sendQuery(
    name: "ReadUIVisionWebsiteConfiguration",
  ): Promise<UIVisionWebsiteConfiguration>;
  sendQuery(
    name: "UIVisionWebsiteControlState" | "UIVisionWebsiteControl",
    data: { uiVisionControl: UIVisionWebsiteControl; url?: string },
  ): Promise<UIVisionWebsiteControlObservation>;
  sendQuery(
    name: "UIVisionLogControlState" | "UIVisionLogControl",
    data: { uiVisionLogControl: UIVisionLogControl },
  ): Promise<UIVisionLogControlObservation>;
  sendQuery(
    name: "RunUIVision",
    data: { url: string; macro: VisionMacro; witnessToken: string },
  ): Promise<UIVisionInvocationObservation>;
  sendQuery(
    name: "WriteAutomaWorkflow",
    data: { workflow: AutomaWorkflow },
  ): Promise<{ storedAuthoredWorkflow: boolean }>;
  sendQuery(
    name: "RunAutoma",
    data: { name: string },
  ): Promise<{ clickedActualExecuteControl: boolean }>;
  sendQuery(
    name: "WebScraperControlState",
    data: { control: ScraperControl },
  ): Promise<ScraperControlState>;
  sendQuery(
    name: "WebScraperControl",
    data: { control: ScraperControl; sitemap?: Sitemap },
  ): Promise<{ value?: string; clickedActualControl?: string }>;
}
export interface AutomationContentWindow extends Window {
  CustomEvent: typeof CustomEvent;
  chrome: {
    storage: {
      local: {
        get(keys: string | string[]): Promise<AutomaStorage>;
        set(value: AutomaStorage): Promise<void>;
      };
    };
  };
}
