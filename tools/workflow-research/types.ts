// SPDX-License-Identifier: MPL-2.0

export const WORKFLOW_VERSION = "floorp-workflow/v1" as const;

export type ContextSource =
  | "api-schema"
  | "browser-state"
  | "dom-structure"
  | "page-text"
  | "runtime-result"
  | "examples";

export interface ContextChunk {
  id: string;
  source: ContextSource;
  title: string;
  text: string;
  tokenEstimate: number;
}

export interface TabsCreateStep {
  action: "tabs.create";
  url: string;
}

export interface TabsNavigateStep {
  action: "tabs.navigate";
  url: string;
}

export interface PageClickStep {
  action: "page.click";
  fingerprint: string;
}

export interface PageFillStep {
  action: "page.fill";
  fingerprint: string;
  value: string;
}

export interface PageWaitForTextStep {
  action: "page.waitForText";
  text: string;
  timeoutMs?: number;
}

export interface PageExtractTextStep {
  action: "page.extractText";
  fingerprint: string;
  as: string;
}

export interface PageAssertTextStep {
  action: "page.assertText";
  text: string;
}

export interface WorkspaceSwitchStep {
  action: "workspace.switch";
  workspaceId: string;
}

export type WorkflowStep =
  | TabsCreateStep
  | TabsNavigateStep
  | PageClickStep
  | PageFillStep
  | PageWaitForTextStep
  | PageExtractTextStep
  | PageAssertTextStep
  | WorkspaceSwitchStep;

export interface Workflow {
  version: typeof WORKFLOW_VERSION;
  steps: WorkflowStep[];
}

export type MissingSource =
  | Exclude<ContextSource, "runtime-result" | "examples">
  | "none";

export interface ValidationDiagnostic {
  code: string;
  message: string;
  severity: "error" | "warning";
  missingSource: MissingSource;
}

export type FailureKind = "schema" | "runtime" | "oracle" | "none";

export interface FailureFeedback {
  kind: Exclude<FailureKind, "none">;
  message: string;
  missingSource: MissingSource;
  round: number;
}

export interface ExtractionOracle {
  type: "extraction";
  as: string;
  includes: string;
}

export interface WorkspaceOracle {
  type: "workspace";
  workspaceId: string;
}

export interface PageTextOracle {
  type: "page-text";
  includes: string;
}

export type OracleSpec = ExtractionOracle | WorkspaceOracle | PageTextOracle;

export interface ResearchTask {
  id: string;
  instruction: string;
  startUrl: string;
  oracle: OracleSpec;
  goldContextIds: string[];
}

export interface SelectorInput {
  task: ResearchTask;
  chunks: ContextChunk[];
  acquiredIds: ReadonlySet<string>;
  feedback: FailureFeedback[];
  round: number;
}

export interface SelectorResult {
  chunkIds: string[];
  selectorTokens: number;
  rationale: string;
}

export interface Selector {
  name: string;
  select(input: SelectorInput): SelectorResult | Promise<SelectorResult>;
}

export interface GenerationInput {
  task: ResearchTask;
  contexts: ContextChunk[];
  feedback: FailureFeedback[];
  round: number;
}

export interface GenerationOutput {
  workflow: unknown;
  generationTokens: number;
  notes: string[];
}

export interface WorkflowGenerator {
  name: string;
  generate(input: GenerationInput): GenerationOutput;
}

export interface ExecutionResult {
  success: boolean;
  failureKind: FailureKind;
  message: string;
  missingSource: MissingSource;
  extracted: Record<string, string>;
  finalUrl: string;
  activeWorkspaceId: string | null;
}

export interface RoundLog {
  round: number;
  contextIds: string[];
  contextTokens: number;
  selectorTokens: number;
  generationTokens: number;
  validationDiagnostics: ValidationDiagnostic[];
  execution: ExecutionResult;
  generatorNotes: string[];
}

export interface ExperimentLog {
  taskId: string;
  selector: string;
  generator: string;
  success: boolean;
  rounds: RoundLog[];
  totalContextTokens: number;
  totalSelectorTokens: number;
  totalGenerationTokens: number;
  totalTokens: number;
  finalFailureKind: FailureKind;
  finalMessage: string;
}
