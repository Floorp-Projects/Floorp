// SPDX-License-Identifier: MPL-2.0
/// <reference types="jsr:@types/gecko" />
// Narrow types for calls to the authentic Vue development build, not a runtime shim.
export interface VueRef {
  value: number;
}
export interface VueComponent<Props extends object> {
  name: string;
  props?: { count: NumberConstructor };
  setup(props: Props): () => object;
}
export interface VueRuntime {
  version: string;
  ref(value: number): VueRef;
  h(type: string, props: object | null, children?: object[] | string): object;
  h<Props extends object>(component: VueComponent<Props>, props: Props): object;
  createApp(
    component: VueComponent<Record<string, never>>,
  ): { mount(target: Element): unknown };
}
export interface VueFixtureGlobal {
  Vue: VueRuntime;
  document: Document;
}
export interface VueCountProps {
  count: number;
}
export interface FrozenFrameworkPackage {
  version: string;
  integrity: string;
  sha256: string;
  metadataSource: string;
  tarball: string;
  verifiedRegistrySignatures: {
    keyid: string;
    key: string;
    signature: string;
  }[];
}
export interface FrameworkReference {
  packages: Record<string, FrozenFrameworkPackage>;
}
export interface VueFrameworkMetadata {
  source: string;
  packages: { vue: FrozenFrameworkPackage & { vendorBundleSha256: string } };
  htmlSha256: string;
  appSha256: string;
  authoredSourceSha256: string;
}
