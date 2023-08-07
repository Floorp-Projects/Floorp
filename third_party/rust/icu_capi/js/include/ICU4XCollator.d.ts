import { FFIError } from "./diplomat-runtime"
import { ICU4XCollatorOptionsV1 } from "./ICU4XCollatorOptionsV1";
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";
import { ICU4XLocale } from "./ICU4XLocale";
import { ICU4XOrdering } from "./ICU4XOrdering";

/**

 * See the {@link https://docs.rs/icu/latest/icu/collator/struct.Collator.html Rust documentation for `Collator`} for more information.
 */
export class ICU4XCollator {

  /**

   * Construct a new Collator instance.

   * See the {@link https://docs.rs/icu/latest/icu/collator/struct.Collator.html#method.try_new_unstable Rust documentation for `try_new_unstable`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_v1(provider: ICU4XDataProvider, locale: ICU4XLocale, options: ICU4XCollatorOptionsV1): ICU4XCollator | never;

  /**

   * Compare potentially ill-formed UTF-8 strings.

   * Ill-formed input is compared as if errors had been replaced with REPLACEMENT CHARACTERs according to the WHATWG Encoding Standard.

   * See the {@link https://docs.rs/icu/latest/icu/collator/struct.Collator.html#method.compare_utf8 Rust documentation for `compare_utf8`} for more information.
   */
  compare(left: string, right: string): ICU4XOrdering;

  /**

   * Compare guaranteed well-formed UTF-8 strings.

   * Note: In C++, passing ill-formed UTF-8 strings is undefined behavior (and may be memory-unsafe to do so, too).

   * See the {@link https://docs.rs/icu/latest/icu/collator/struct.Collator.html#method.compare Rust documentation for `compare`} for more information.
   */
  compare_valid_utf8(left: string, right: string): ICU4XOrdering;

  /**

   * Compare potentially ill-formed UTF-16 strings, with unpaired surrogates compared as REPLACEMENT CHARACTER.

   * See the {@link https://docs.rs/icu/latest/icu/collator/struct.Collator.html#method.compare_utf16 Rust documentation for `compare_utf16`} for more information.
   */
  compare_utf16(left: Uint16Array, right: Uint16Array): ICU4XOrdering;
}
