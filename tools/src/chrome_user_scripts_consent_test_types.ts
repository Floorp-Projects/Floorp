export interface Manifest {
  manifest_version: number;
  permissions?: string[];
  optional_permissions?: string[];
  browser_specific_settings?: {
    gecko: {
      floorp_chrome_extension: { schema_version: number; original_id: string };
    };
  };
}
export interface Extension {
  id: string;
  manifest: Manifest;
  permissions: Set<string>;
  hasPermission(name: string): boolean;
}
export interface Status {
  eligible: boolean;
  allowed: boolean;
  permissionGranted: boolean;
}
export interface Consent {
  isEligible(extension: Pick<Extension, "id" | "manifest">): boolean;
  isAllowed(extension: Extension): boolean;
  getStatus(id: string): Promise<Status>;
  setAllowed(id: string, value: boolean): Promise<Status>;
}
export interface Addon {
  id: string;
  version: string;
  type: string;
  getResourceURI(path: string): { spec: string };
}
export interface AddonListener {
  onInstalling(addon: Addon): void;
  onInstalled(addon: Addon): void;
  onUninstalling(addon: Addon): void;
  onUninstalled(addon: Addon): void;
}
export interface Deferred {
  promise: Promise<void>;
  resolve(): void;
}
