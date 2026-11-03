// SPDX-License-Identifier: MPL-2.0
export interface ProxyTestManifest {
  browser_specific_settings?: {
    gecko?: {
      floorp_chrome_extension?: { schema_version: number; original_id: string };
    };
  };
}
export interface ProxyTestExtension {
  id: string;
  manifest: ProxyTestManifest;
  privateBrowsingAllowed: boolean;
  baseURL: string;
}
export interface ProxyTestAddon {
  id: string;
  manifest: ProxyTestManifest;
  isActive: boolean;
  installDate: Date;
  getResourceURI(): string;
}
export interface ProxyNativeConfiguration {
  proxyType: string;
  autoConfigUrl?: string;
}
export interface ProxyScopeRecord {
  id: string;
  kind: string;
  chrome: boolean;
  value: ProxyNativeConfiguration;
}
export type ProxyNativeSet = (
  details: { value: ProxyNativeConfiguration },
) => Promise<void>;
export type ProxyNativeClear = (details: object) => Promise<void>;
export interface ProxyNativeScopeConfiguration {
  key: string;
  type: number;
  PACURI: string;
  mandatory: boolean;
  allowLoopback: boolean;
}
export interface ProxyTestChannel {
  loadInfo: { originAttributes: { privateBrowsingId: number } };
  QueryInterface(interfaceId: unknown): { isChannelPrivate: boolean };
}
export interface ProxyScopeEngine {
  state: {
    regular: ProxyScopeRecord | null;
    private: ProxyScopeRecord | null;
  };
  startupManifests: Map<string, unknown>;
  refresh(): Promise<void>;
  configuration(
    record: ProxyScopeRecord,
    privateContext: boolean,
  ): ProxyNativeScopeConfiguration;
  provider: {
    getConfiguration(
      channel: ProxyTestChannel,
      callback: {
        onConfiguration(config: ProxyNativeScopeConfiguration | null): void;
      },
    ): boolean;
  };
  handles(record: ProxyScopeRecord | null, privateContext: boolean): boolean;
  selected(privateContext: boolean): ProxyScopeRecord | null;
  set(
    extension: ProxyTestExtension,
    details: { value: unknown; scope: string },
    nativeSet: ProxyNativeSet,
    nativeClear: ProxyNativeClear,
  ): Promise<void>;
  clear(
    extension: ProxyTestExtension,
    details: { scope: string },
    nativeClear: ProxyNativeClear,
  ): Promise<void>;
}
export interface ProxyVMContext {
  [key: string]: unknown;
  scopes?: ProxyScopeEngine;
}
