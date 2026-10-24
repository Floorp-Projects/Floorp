// SPDX-License-Identifier: MPL-2.0
/// <reference types="jsr:@types/gecko" />

import type {
  ChromeConversionReport,
  ChromeManifest,
} from "../../browser-features/modules/modules/chrome-web-store/types.ts";

export interface NativeCorpusActor {
  sendQuery<Result = unknown>(name: string, data?: object): Promise<Result>;
}

export interface NativeCorpusBrowser {
  remoteType: string;
  contentTitle: string;
  currentURI: nsIURI;
  browsingContext: {
    currentWindowGlobal: {
      innerWindowId: number;
      isCurrentGlobal: boolean;
      documentPrincipal: nsIPrincipal;
      documentURI: nsIURI;
      getActor(name: string): NativeCorpusActor;
    } | null;
  };
  messageManager: {
    loadFrameScript(source: string, allFrames: boolean): void;
  };
  reload(): void;
  loadURI(uri: nsIURI, options: { triggeringPrincipal: nsIPrincipal }): void;
}

export interface NativeCorpusTab {
  linkedBrowser: NativeCorpusBrowser;
}

export interface NativePermissionNotification extends XULElement {
  button?: HTMLButtonElement;
}

export interface NativeCorpusWindow extends Window {
  document: Document;
  wrappedJSObject: { __floorpRuntimeFixtureURL: string };
  gBrowser: {
    tabs: NativeCorpusTab[];
    selectedTab: NativeCorpusTab;
    addTab(
      url: string,
      options: { triggeringPrincipal: nsIPrincipal },
    ): NativeCorpusTab;
    removeTab(
      tab: NativeCorpusTab,
      options?: { skipPermitUnload: boolean },
    ): void;
  };
  PopupNotifications: {
    panel: XULElement & { state: string };
  };
}

export interface NativeCorpusAddon {
  isActive: boolean;
  userDisabled: boolean;
  uninstall(): Promise<void>;
}

export interface NativeCorpusBackground {
  viewType: string;
  xulBrowser?: NativeCorpusBrowser;
  unloaded?: boolean;
}

export interface NativeCorpusExtension {
  id: string;
  baseURI: nsIURI;
  principal: nsIPrincipal;
  backgroundContext?: NativeCorpusBackground;
  views: Set<NativeCorpusBackground>;
  apiManager: {
    asyncGetAPI(
      name: string,
      extension: NativeCorpusExtension,
      scope: string,
    ): Promise<{
      widget?: { id: string };
      action: { getProperty(tab: NativeCorpusTab, property: string): unknown };
    }>;
  };
}

export interface NativeCorpusExtensionParent {
  GlobalManager: { extensionMap: Map<string, NativeCorpusExtension> };
}

export interface InstalledFrozenCorpus {
  addon: NativeCorpusAddon;
  originalManifest: ChromeManifest;
  unchangedScriptCount: number;
  unchangedScriptsSha256: string;
  warnings?: string[];
  background?: ChromeConversionReport["background"];
  conversion?: ChromeConversionReport;
  extension: NativeCorpusExtension;
  sourceIntegrity: FrozenCorpusScriptEvidence;
  nativeEvidence: NativeCorpusBackgroundEvidence;
}

export interface FrozenCorpusScriptEvidence {
  unchangedScriptCount: number;
  unchangedScriptsSha256: string;
  originalServiceWorker: string | null;
  convertedServiceWorker: string | null;
}

export interface NativeCorpusBackgroundInput {
  background?: ChromeConversionReport["background"];
  originalServiceWorker: string | null;
  convertedServiceWorker: string | null;
}

export interface NativeCorpusBackgroundSnapshot
  extends NativeCorpusBackgroundInput {
  runId: string | null;
  strictNative: boolean;
  version: string;
  buildId: string;
  workerWebIDLCompiled: boolean;
  backgroundViewType: string | null;
  candidateIdentity?: NativeCorpusCandidateIdentity;
}

export interface NativeCorpusCandidateIdentity {
  manifestSha256: string;
  packageSha256: string;
  sourceCommit: string;
  floorpCommit: string;
  patches: Record<string, string>;
  buildId: string;
  platform: string;
}

export interface NativeCorpusBackgroundEvidence
  extends NativeCorpusBackgroundSnapshot {
  nativeQualified: false;
  buildScope: "strict-native-candidate" | "historical-nonqualifying";
  nativeWorkerVerified: boolean;
  candidateIdentityVerified: boolean;
  nativeBackgroundVerified: boolean;
}
