import { FFIError } from "./diplomat-runtime"
import { ICU4XError } from "./ICU4XError";
import { ICU4XLocaleFallbacker } from "./ICU4XLocaleFallbacker";

/**

 * An ICU4X data provider, capable of loading ICU4X data keys from some source.

 * See the {@link https://docs.rs/icu_provider/latest/icu_provider/index.html Rust documentation for `icu_provider`} for more information.
 */
export class ICU4XDataProvider {

  /**

   * Constructs an `FsDataProvider` and returns it as an {@link ICU4XDataProvider `ICU4XDataProvider`}. Requires the `provider_fs` Cargo feature. Not supported in WASM.

   * See the {@link https://docs.rs/icu_provider_fs/latest/icu_provider_fs/struct.FsDataProvider.html Rust documentation for `FsDataProvider`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_fs(path: string): ICU4XDataProvider | never;

  /**

   * Constructs a testdata provider and returns it as an {@link ICU4XDataProvider `ICU4XDataProvider`}. Requires the `provider_test` and one of `any_provider` or `buffer_provider` Cargo features.

   * See the {@link https://docs.rs/icu_testdata/latest/icu_testdata/index.html Rust documentation for `icu_testdata`} for more information.
   */
  static create_test(): ICU4XDataProvider;

  /**

   * Constructs a `BlobDataProvider` and returns it as an {@link ICU4XDataProvider `ICU4XDataProvider`}.

   * See the {@link https://docs.rs/icu_provider_blob/latest/icu_provider_blob/struct.BlobDataProvider.html Rust documentation for `BlobDataProvider`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_from_byte_slice(blob: Uint8Array): ICU4XDataProvider | never;

  /**

   * Constructs an empty {@link ICU4XDataProvider `ICU4XDataProvider`}.

   * See the {@link https://docs.rs/icu_provider_adapters/latest/icu_provider_adapters/empty/struct.EmptyDataProvider.html Rust documentation for `EmptyDataProvider`} for more information.
   */
  static create_empty(): ICU4XDataProvider;

  /**

   * Creates a provider that tries the current provider and then, if the current provider doesn't support the data key, another provider `other`.

   * This takes ownership of the `other` provider, leaving an empty provider in its place.

   * The providers must be the same type (Any or Buffer). This condition is satisfied if both providers originate from the same constructor, such as `create_from_byte_slice` or `create_fs`. If the condition is not upheld, a runtime error occurs.

   * See the {@link https://docs.rs/icu_provider_adapters/latest/icu_provider_adapters/fork/type.ForkByKeyProvider.html Rust documentation for `ForkByKeyProvider`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  fork_by_key(other: ICU4XDataProvider): void | never;

  /**

   * Same as `fork_by_key` but forks by locale instead of key.

   * See the {@link https://docs.rs/icu_provider_adapters/latest/icu_provider_adapters/fork/predicates/struct.MissingLocalePredicate.html Rust documentation for `MissingLocalePredicate`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  fork_by_locale(other: ICU4XDataProvider): void | never;

  /**

   * Enables locale fallbacking for data requests made to this provider.

   * Note that the test provider (from `create_test`) already has fallbacking enabled.

   * See the {@link https://docs.rs/icu_provider_adapters/latest/icu_provider_adapters/fallback/struct.LocaleFallbackProvider.html#method.try_new_unstable Rust documentation for `try_new_unstable`} for more information.

   * Additional information: {@link https://docs.rs/icu_provider_adapters/latest/icu_provider_adapters/fallback/struct.LocaleFallbackProvider.html 1}
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  enable_locale_fallback(): void | never;

  /**

   * See the {@link https://docs.rs/icu_provider_adapters/latest/icu_provider_adapters/fallback/struct.LocaleFallbackProvider.html#method.new_with_fallbacker Rust documentation for `new_with_fallbacker`} for more information.

   * Additional information: {@link https://docs.rs/icu_provider_adapters/latest/icu_provider_adapters/fallback/struct.LocaleFallbackProvider.html 1}
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  enable_locale_fallback_with(fallbacker: ICU4XLocaleFallbacker): void | never;
}
