// SPDX-License-Identifier: MPL-2.0
import type {
  ContentSnapshot,
  CorpusOperation,
  CorpusReport,
} from "./assessment-test-types.ts";
import type { NativeCorpusExtension } from "./native-test-types.ts";
import type { UserScriptsConsentUIEvidence } from "../../browser-features/modules/modules/chrome-web-store/test/helpers/user-scripts-consent-test-types.ts";

export type ContentUserScriptsDeclaration = "required" | "optional";

export interface ContentUserScriptsConsent {
  declaration: ContentUserScriptsDeclaration;
  originalManifestVersion: 3;
  chromeId: string;
  addonId: string;
  ui: UserScriptsConsentUIEvidence;
}

export interface ContentChromeIdentityModule {
  ExtensionUtils: {
    getChromeExtensionId(extension: NativeCorpusExtension): string | null;
  };
}

export interface ContentCorpusOperation
  extends CorpusOperation<ContentSnapshot> {
  userScriptsConsent?: ContentUserScriptsConsent | null;
}

export interface ContentCorpusReport extends CorpusReport<ContentSnapshot> {
  extensions: ContentCorpusOperation[];
}
