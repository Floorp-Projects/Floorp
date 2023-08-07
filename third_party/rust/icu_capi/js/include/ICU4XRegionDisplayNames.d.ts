import { FFIError } from "./diplomat-runtime"
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";
import { ICU4XLocale } from "./ICU4XLocale";

/**

 * See the {@link https://docs.rs/icu/latest/icu/displaynames/struct.RegionDisplayNames.html Rust documentation for `RegionDisplayNames`} for more information.
 */
export class ICU4XRegionDisplayNames {

  /**

   * Creates a new `RegionDisplayNames` from locale data and an options bag.

   * See the {@link https://docs.rs/icu/latest/icu/displaynames/struct.RegionDisplayNames.html#method.try_new_unstable Rust documentation for `try_new_unstable`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static try_new_unstable(provider: ICU4XDataProvider, locale: ICU4XLocale): ICU4XRegionDisplayNames | never;

  /**

   * Returns the locale specific display name of a region. Note that the funtion returns an empty string in case the display name for a given region code is not found.

   * See the {@link https://docs.rs/icu/latest/icu/displaynames/struct.RegionDisplayNames.html#method.of Rust documentation for `of`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  of(region: string): string | never;
}
