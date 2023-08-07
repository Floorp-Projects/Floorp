import { u32, char } from "./diplomat-runtime"
import { FFIError } from "./diplomat-runtime"
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";
import { ICU4XLocale } from "./ICU4XLocale";

/**

 * An ICU4X Unicode Set Property object, capable of querying whether a code point is contained in a set based on a Unicode property.

 * See the {@link https://docs.rs/icu/latest/icu/properties/index.html Rust documentation for `properties`} for more information.

 * See the {@link https://docs.rs/icu/latest/icu/properties/sets/struct.UnicodeSetData.html Rust documentation for `UnicodeSetData`} for more information.

 * See the {@link https://docs.rs/icu/latest/icu/properties/sets/struct.UnicodeSetDataBorrowed.html Rust documentation for `UnicodeSetDataBorrowed`} for more information.
 */
export class ICU4XUnicodeSetData {

  /**

   * Checks whether the string is in the set.

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/struct.UnicodeSetDataBorrowed.html#method.contains Rust documentation for `contains`} for more information.
   */
  contains(s: string): boolean;

  /**

   * Checks whether the code point is in the set.

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/struct.UnicodeSetDataBorrowed.html#method.contains_char Rust documentation for `contains_char`} for more information.
   */
  contains_char(cp: char): boolean;

  /**

   * Checks whether the code point (specified as a 32 bit integer, in UTF-32) is in the set.
   */
  contains32(cp: u32): boolean;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_basic_emoji.html Rust documentation for `load_basic_emoji`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_basic_emoji(provider: ICU4XDataProvider): ICU4XUnicodeSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/exemplar_chars/fn.load_exemplars_main.html Rust documentation for `load_exemplars_main`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_exemplars_main(provider: ICU4XDataProvider, locale: ICU4XLocale): ICU4XUnicodeSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/exemplar_chars/fn.load_exemplars_auxiliary.html Rust documentation for `load_exemplars_auxiliary`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_exemplars_auxiliary(provider: ICU4XDataProvider, locale: ICU4XLocale): ICU4XUnicodeSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/exemplar_chars/fn.load_exemplars_punctuation.html Rust documentation for `load_exemplars_punctuation`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_exemplars_punctuation(provider: ICU4XDataProvider, locale: ICU4XLocale): ICU4XUnicodeSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/exemplar_chars/fn.load_exemplars_numbers.html Rust documentation for `load_exemplars_numbers`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_exemplars_numbers(provider: ICU4XDataProvider, locale: ICU4XLocale): ICU4XUnicodeSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/exemplar_chars/fn.load_exemplars_index.html Rust documentation for `load_exemplars_index`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_exemplars_index(provider: ICU4XDataProvider, locale: ICU4XLocale): ICU4XUnicodeSetData | never;
}
