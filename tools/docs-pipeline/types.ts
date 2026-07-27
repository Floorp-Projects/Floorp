// SPDX-License-Identifier: MPL-2.0

export type SourceRef = {
  path: string;
  line?: number;
};

export type DenoTaskEntry = {
  name: string;
  command: string;
  source: SourceRef;
};

export type FelesCommandEntry = {
  name: string;
  usage?: string;
  source: SourceRef;
};

export type ArchitectureInventory = {
  layers: Array<{
    name: string;
    source: SourceRef;
    summary: string;
  }>;
  referenceSources: Array<{
    area: string;
    source: SourceRef;
    summary: string;
  }>;
  chromeFeatureDiscovery: {
    globPattern: string;
    source: SourceRef;
  };
  windowActors: {
    registrationApi: string;
    actorCount: number;
    source: SourceRef;
  };
  bridgeLoader: {
    devLoaderUrl: string;
    testLoaderUrl: string;
    productionLoader: string;
    source: SourceRef;
  };
  loaderDevServer: {
    port: number;
    source: SourceRef;
  };
};

export type CiWorkflowEntry = {
  name: string;
  path: string;
  triggers: string[];
  permissions: string[];
  runCommands: string[];
};

export type FeatureCatalogEntry = {
  name: string;
  source: SourceRef;
  summary: string;
  entrypoints: string[];
};

export type SettingsRouteEntry = {
  route: string;
  component: string;
  source: SourceRef;
};

export type WindowActorEntry = {
  name: string;
  source: SourceRef;
};

export type OsApiRouteEntry = {
  method: "GET" | "POST" | "DELETE";
  path: string;
  source: SourceRef;
  summary: string;
};

export type FloorpOsApiInventory = {
  server: SourceRef;
  router: SourceRef;
  sharedAutomationRoutes: SourceRef;
  automotorManager: SourceRef;
  settingsPage: SourceRef;
  verification: SourceRef[];
  routeModules: Array<{
    namespace: string;
    source: SourceRef;
    routes: OsApiRouteEntry[];
  }>;
};

export type DocsInventory = {
  schemaVersion: 1;
  generatedAt: string;
  floorpCommit: string;
  sourcePrecedence: string[];
  commands: {
    denoTasks: DenoTaskEntry[];
    felesBuild: FelesCommandEntry[];
  };
  architecture: ArchitectureInventory;
  ci: {
    workflows: CiWorkflowEntry[];
  };
  features: {
    chromeCommon: FeatureCatalogEntry[];
    chromeStatic: FeatureCatalogEntry[];
    settingsRoutes: SettingsRouteEntry[];
    windowActors: WindowActorEntry[];
  };
  floorpOsApi: FloorpOsApiInventory;
  knownDriftChecks: string[];
};

export type GeneratedPage = {
  path: string;
  title: string;
  sidebar_label: string;
  body: string;
};

export type GeneratedDocsPayload = {
  pages: GeneratedPage[];
};

export const DETERMINISTIC_GENERATED_PAGE_PATHS = [
  "development/architecture-overview.mdx",
  "development/directories/static-gecko.mdx",
  "development/directories/browser-features/chrome/common.mdx",
  "development/directories/floorp-os-api.mdx",
  "development/features/browser-features/overview.mdx",
  "development/features/browser-features/chrome-common.mdx",
  "development/features/browser-features/chrome-static.mdx",
  "development/features/browser-features/settings-pages.mdx",
  "development/features/browser-features/window-actors.mdx",
  "development/features/browser-features/common/overview.mdx",
  "development/features/browser-features/common/tabs-and-workspaces.mdx",
  "development/features/browser-features/common/sidebar-and-panels.mdx",
  "development/features/browser-features/common/browser-ui-customization.mdx",
  "development/features/browser-features/common/input-and-shortcuts.mdx",
  "development/features/browser-features/common/webapps-and-integration.mdx",
  "development/features/browser-features/common/utilities-and-actions.mdx",
  "development/features/browser-features/modules/overview.mdx",
  "development/features/browser-features/modules/settings-and-internal-pages-actors.mdx",
  "development/features/browser-features/modules/web-content-and-store-actors.mdx",
  "development/features/browser-features/modules/pwa-workspaces-profile-actors.mdx",
  "development/reference/source-inventory.mdx",
  "development/reference/command-reference.mdx",
  "development/reference/ci-test-reference.mdx",
] as const;

export const REQUIRED_GENERATED_PAGE_PATHS = [
  "development/directories/bridge.mdx",
  "development/directories/browser-features/overview.mdx",
  "development/directories/browser-features/chrome/overview.mdx",
  "development/directories/browser-features/chrome/static.mdx",
  "development/directories/browser-features/modules/overview.mdx",
  "development/directories/browser-features/modules/browser-glue.mdx",
  "development/directories/browser-features/pages-settings/overview.mdx",
  "development/directories/browser-features/pages-settings/build.mdx",
  "development/directories/browser-features/pages-settings/routing.mdx",
  "development/directories/tools-and-ci.mdx",
  ...DETERMINISTIC_GENERATED_PAGE_PATHS,
] as const;
