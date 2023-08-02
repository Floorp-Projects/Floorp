import { u8, u32, char } from "./diplomat-runtime"
import { FFIError } from "./diplomat-runtime"
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";

/**

 * Lookup of the Canonical_Combining_Class Unicode property

 * See the {@link https://docs.rs/icu/latest/icu/normalizer/properties/struct.CanonicalCombiningClassMap.html Rust documentation for `CanonicalCombiningClassMap`} for more information.
 */
export class ICU4XCanonicalCombiningClassMap {

  /**

   * Construct a new ICU4XCanonicalCombiningClassMap instance for NFC

   * See the {@link https://docs.rs/icu/latest/icu/normalizer/properties/struct.CanonicalCombiningClassMap.html#method.try_new_unstable Rust documentation for `try_new_unstable`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create(provider: ICU4XDataProvider): ICU4XCanonicalCombiningClassMap | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/normalizer/properties/struct.CanonicalCombiningClassMap.html#method.get Rust documentation for `get`} for more information.

   * Additional information: {@link https://docs.rs/icu/latest/icu/properties/properties/struct.CanonicalCombiningClass.html 1}
   */
  get(ch: char): u8;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/normalizer/properties/struct.CanonicalCombiningClassMap.html#method.get32 Rust documentation for `get32`} for more information.

   * Additional information: {@link https://docs.rs/icu/latest/icu/properties/properties/struct.CanonicalCombiningClass.html 1}
   */
  get32(ch: u32): u8;
}
