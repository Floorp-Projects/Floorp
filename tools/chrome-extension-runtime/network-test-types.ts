// SPDX-License-Identifier: MPL-2.0
/// <reference types="jsr:@types/gecko" />
import type { ChromeConversionReport } from "../../browser-features/modules/modules/chrome-web-store/types.ts";
import type { FrozenCorpusExtension } from "./corpus-types.ts";
import type {
  NativeCorpusWindow,
  NativeCorpusTab,
  NativeCorpusExtension,
  NativeCorpusActor,
  NativeCorpusBackgroundEvidence,
  NativeCorpusCandidateIdentity,
} from "./native-test-types.ts";
import type { ChromePipe } from "./chrome-cdp-pipe.mjs";

export interface FrozenSelection extends FrozenCorpusExtension {
  id: string;
  name: string;
  version: string;
  packageSha256: string;
  primaryOperation: string;
}
export interface NetworkCorpus {
  extensions: FrozenSelection[];
}
export interface ChromeReference {
  browserVersion: string;
  binary: { sha256: string };
}
export interface CdpVersion {
  product: string;
}
export interface CdpTarget {
  targetId: string;
  type: string;
  url: string;
}
export interface TargetList {
  targetInfos: CdpTarget[];
}
export interface Evaluation<T> {
  result: { value: T };
  exceptionDetails?: unknown;
}
export interface Point {
  x: number;
  y: number;
}
export interface PageSession {
  targetId: string;
  session: string;
}
export type Until = <T>(
  read: () =>
    | T
    | false
    | null
    | undefined
    | Promise<T | false | null | undefined>,
  label: string,
  timeout?: number,
) => Promise<T>;
export interface TrafficRecord {
  role: string;
  url: string;
  headers: Record<string, string | string[] | undefined>;
}
export interface NetworkFetch {
  role?: string;
  headers?: Record<string, string | undefined>;
  failed?: boolean;
  error?: string;
}
export interface ProxyReadback {
  value: { mode: string; [key: string]: unknown };
  levelOfControl?: string;
}
export interface UISnapshot {
  url: string;
  ready: string;
  text: string;
  controls?: {
    tag: string;
    id: string;
    name?: string;
    value?: string;
    text?: string;
    model?: string | null;
    data?: Record<string, string | undefined>;
  }[];
}
export interface SourceIntegrity {
  unchangedScriptCount: number;
  unchangedScriptsSha256: string;
  [key: string]: unknown;
}
export interface NetworkRecord extends Omit<
  FrozenSelection,
  "status" | "area"
> {
  primaryOperationResult: string;
  nativeQualified?: boolean;
  nativeEvidence?: NativeCorpusBackgroundEvidence;
  checks: string[];
  trace: UISnapshot[];
  error?: string;
  stage?: string;
  baselineAdLoaded?: boolean;
  sourceIntegrity?: unknown;
  routed?: NetworkFetch;
  bypassed?: NetworkFetch;
  restored?: NetworkFetch;
  restoredConfig?: ProxyReadback;
  echo?: NetworkFetch;
  blocked?: boolean;
  productDNR?: unknown;
  proxyAfterSelection?: ProxyReadback;
  targets?: CdpTarget[];
  uninstallError?: string;
  conversion?: ChromeConversionReport;
  warnings?: string[];
  unchangedScriptCount?: number;
  unchangedScriptsSha256?: string;
  background?: unknown;
  backgroundViewType?: string | null;
  nativeWorkerVerified?: boolean;
  actionDiagnostics?: unknown;
}
export interface NetworkReport {
  nativeQualified?: boolean;
  nativeCandidateIdentity?: NativeCorpusCandidateIdentity | null;
  qualificationMode?: "candidate" | "historical";
  stage: string;
  browserMode: string;
  fixture: string;
  proxyPort: number | string;
  primaryOperationPassRate: null;
  extensions: NetworkRecord[];
  output: string;
  reference?: ChromeReference;
  version?: CdpVersion;
  traffic?: TrafficRecord[];
  runtime?: {
    version: string;
    buildId: string;
    nativeWorkerPolicy: boolean;
    workerWebIDLCompiled: boolean;
  };
}
export interface NativeActor extends NativeCorpusActor {
  sendQuery(name: "Snapshot", data?: UIControl): Promise<UISnapshot>;
  sendQuery(name: "ProxySettings", data?: UIControl): Promise<ProxyReadback>;
  sendQuery(name: "Fetch", data: { url: string }): Promise<NetworkFetch>;
  sendQuery(name: "Exists" | "AdState", data?: UIControl): Promise<boolean>;
  sendQuery(name: "Click" | "Fill", data?: UIControl): Promise<unknown>;
}
export interface UIControl {
  selector?: string;
  text?: string;
  value?: string;
  optional?: boolean;
  menuText?: string;
  buttonText?: string;
}
export type NativeTab = NativeCorpusTab;
export type NativeBrowserWindow = NativeCorpusWindow;
export interface NativePage {
  tab?: NativeTab;
  actor(): NativeActor;
}
export interface NativeTabPage extends NativePage {
  tab: NativeTab;
}
export interface NativeExtension extends NativeCorpusExtension {
  manifest: { background?: unknown };
}
export interface InstalledAddon {
  id: string;
  uninstall(): Promise<void>;
}
export type FormElement = HTMLElement & {
  name?: string;
  value?: string;
  labels?: NodeListOf<HTMLLabelElement> | null;
};
export interface NetworkContentWindow extends Window {
  chrome: {
    proxy: {
      settings: { get(options: Record<string, never>): Promise<ProxyReadback> };
    };
  };
  __corpusAdLoaded?: boolean;
}
export interface NetworkActorMessage {
  name: string;
  data?: UIControl & { url?: string };
}
export interface LanguageProduct {
  tag: string;
  cls: string;
  text?: string;
  html: string;
}
export interface LanguageSnapshot {
  url: string;
  ready: string;
  origin?: number;
  installed: string | null;
  text?: string;
  body: string;
  product: LanguageProduct[];
}
export interface NoticeSnapshot {
  url: string;
  ready: string;
  text: string;
}
export interface ServiceRequest {
  url: string;
  method?: string;
  status?: number;
  topic?: string;
  addonId?: string;
  source?: number;
}
export interface LanguageItem extends Omit<FrozenSelection, "status" | "area"> {
  primaryOperationResult: string;
  checks: string[];
  sourceIntegrity?: unknown;
  baseline?: LanguageSnapshot;
  typed?: LanguageSnapshot;
  suggestions?: LanguageSnapshot;
  lastSnapshot?: LanguageSnapshot | null;
  transformed?: { errors: LanguageSnapshot; corrected: LanguageSnapshot };
  afterNavigation?: { errors: LanguageSnapshot; corrected: LanguageSnapshot };
  afterRemoval?: LanguageSnapshot;
  error?: string;
  onboarding?: NoticeSnapshot[] | LanguageSnapshot;
  onboardingPending?: NoticeSnapshot;
  remoteSetupAttempt?: unknown;
  consent?: { notice: string; nativeControl: string; scope: string };
  lastInput?: UIControl & { operation: string };
  nativeEvents?: unknown[];
  serviceRequests?: ServiceRequest[];
  serviceResponses?: { url: string; status: number }[];
  nativeServiceRequests?: ServiceRequest[];
  nativeTargets?: unknown;
  conversion?: ChromeConversionReport;
  warnings?: string[];
  background?: unknown;
  nativeWorkerVerified?: boolean;
}
export interface LanguageFixture {
  url: string;
  text: string;
  sourceIntegrity?: unknown;
  onboardingHost: { url: string };
}
export interface LanguageReport {
  stage: string;
  fixture: LanguageFixture;
  extensions: LanguageItem[];
  primaryOperationPassRate: null;
  output: string;
  reference?: ChromeReference;
  displayMode?: string;
  version?: CdpVersion;
  runtime?: { version: string; buildId: string; nativeWorkerCompiled: boolean };
}
export interface LanguageDriver {
  item: LanguageItem;
  snapshot(): Promise<LanguageSnapshot>;
  until: Until;
  type(text: string): Promise<unknown>;
  click(control: UIControl): Promise<unknown>;
  navigate(url: string): Promise<unknown>;
  pause(ms: number): Promise<unknown>;
  url: string;
  text: string;
}
export interface LanguageResults {
  Snapshot: LanguageSnapshot;
  Click: unknown;
  Type: unknown;
}
export interface LanguageActor {
  sendQuery<N extends keyof LanguageResults>(
    name: N,
    data?: UIControl,
  ): Promise<LanguageResults[N]>;
}
export interface ResourceProtocol extends Omit<
  nsIResProtocolHandler,
  "setSubstitution"
> {
  setSubstitution(root: string, uri: nsIURI | null): void;
}
export interface LanguageActorMessage {
  name: string;
  data?: UIControl;
}
export interface NativeNetLog {
  events: {
    params?: { url?: string; method?: string };
    source?: { id: number };
  }[];
}
export type MediaSnapshot = import("./assessment-test-types.ts").MediaSnapshot;
export interface SponsorItem {
  id: string;
  checks: string[];
  lastSnapshot?: MediaSnapshot;
  transformed?: MediaSnapshot;
  afterNavigation?: MediaSnapshot;
  afterRemoval?: MediaSnapshot;
  lateOnboardingTargets?: CdpTarget[];
  lateOnboardingTabs?: { url: string }[];
}
export interface SponsorDriver {
  item: SponsorItem;
  snapshot(): Promise<MediaSnapshot>;
  until: Until;
  play(): Promise<unknown>;
  navigate(url: string): Promise<unknown>;
  url: string;
}
export interface SponsorReferenceDriver extends Omit<SponsorDriver, "play"> {
  browser: ChromePipe;
  targetId: string;
  delay(ms: number): Promise<unknown>;
  readyAndPlay(): Promise<unknown>;
}
export interface SponsorFloorpDriver extends Omit<
  SponsorDriver,
  "play" | "snapshot"
> {
  window: NativeBrowserWindow;
  tab: NativeTab;
  extensionBaseURL: string;
  pause(ms: number): Promise<unknown>;
  query<N extends keyof SponsorQueryResults>(
    name: N,
    data?: UIControl,
  ): Promise<SponsorQueryResults[N]>;
}
export interface HeaderInfo {
  header: string;
  values?: string[];
  excludedValues?: string[];
}
export interface RuleCondition {
  urlFilter?: string;
  regexFilter?: string;
  isUrlFilterCaseSensitive?: boolean;
  resourceTypes?: string[];
  responseHeaders?: HeaderInfo[];
  excludedResponseHeaders?: HeaderInfo[];
  [key: string]: unknown;
}
export interface ModifyHeader {
  header: string;
  operation: string;
  value?: string;
}
export interface RuleAction {
  type: string;
  responseHeaders?: ModifyHeader[];
  requestHeaders?: ModifyHeader[];
  redirect?: { url: string };
}
export interface DnrRule {
  id: number;
  priority?: number;
  condition: RuleCondition;
  action: RuleAction;
}
export interface DnrAPI {
  getDynamicRules(): Promise<DnrRule[]>;
  updateDynamicRules(options: {
    removeRuleIds?: number[];
    addRules?: DnrRule[];
  }): Promise<void>;
  getSessionRules(): Promise<DnrRule[]>;
  updateSessionRules(options: {
    removeRuleIds?: number[];
    addRules?: DnrRule[];
  }): Promise<void>;
  testMatchOutcome(options: {
    url: string;
    type: string;
    responseHeaders?: Record<string, string[]>;
  }): Promise<{ matchedRules: { ruleId: number }[] }>;
  isRegexSupported(options: {
    regex: string;
    requireCapturing?: boolean;
  }): Promise<{ isSupported: boolean; reason?: string }>;
}
export interface HeaderObservation {
  label?: string;
  condition?: RuleCondition;
  headers?: Record<string, string[]>;
  matched?: boolean;
  serverMustSee?: string;
  rejected?: boolean;
  stored?: DnrRule[];
  [key: string]: unknown;
}
export interface HeaderVectorResult {
  checks: string[];
  observations: HeaderObservation[];
}
export interface HeaderVectorError extends Error {
  checks?: string[];
  observations?: HeaderObservation[];
}
export interface HeaderReport extends Partial<HeaderVectorResult> {
  passed: boolean;
  origin: string;
  runtime?: { version: string; buildId: string };
  reference?: ChromeReference;
  browserMode?: string;
  version?: CdpVersion;
  traffic?: {
    path: string;
    headers: Record<string, string | string[] | undefined>;
  }[];
  ordinary?: { rejected: boolean; stored: number };
  error?: string;
}
export interface ProxyTraffic {
  role: string;
  method?: string;
  url: string;
}
export interface ProxyReferenceReport {
  schemaVersion: number;
  version: CdpVersion;
  traffic: ProxyTraffic[];
  result: { http: string; https: string; value: ProxyReadback };
  exception?: unknown;
  credentials?: ProxyTraffic;
  fragment?: ProxyTraffic;
  passed?: boolean;
}

export interface SponsorQueryResults {
  Snapshot: MediaSnapshot;
  Click: unknown;
}
