// SPDX-License-Identifier: MPL-2.0
import type { LanguageItem, LanguageReport } from "./network-test-types.ts";
import type {
  FrozenCorpusScriptEvidence,
  NativeCorpusBackgroundEvidence,
} from "./native-test-types.ts";

export interface NativeLanguageItem extends LanguageItem {
  nativeQualified: boolean;
  cleanupCompleted: boolean;
  cleanupError?: string;
  sourceIntegrity?: FrozenCorpusScriptEvidence;
  nativeEvidence?: NativeCorpusBackgroundEvidence;
}

export interface NativeLanguageReport
  extends Omit<LanguageReport, "extensions" | "runtime"> {
  extensions: NativeLanguageItem[];
  runtime: { version: string; buildId: string; nativeWorkerCompiled: boolean };
}
