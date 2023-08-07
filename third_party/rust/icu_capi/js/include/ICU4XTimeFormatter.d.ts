import { FFIError } from "./diplomat-runtime"
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XDateTime } from "./ICU4XDateTime";
import { ICU4XError } from "./ICU4XError";
import { ICU4XIsoDateTime } from "./ICU4XIsoDateTime";
import { ICU4XLocale } from "./ICU4XLocale";
import { ICU4XTime } from "./ICU4XTime";
import { ICU4XTimeLength } from "./ICU4XTimeLength";

/**

 * An ICU4X TimeFormatter object capable of formatting an {@link ICU4XTime `ICU4XTime`} type (and others) as a string

 * See the {@link https://docs.rs/icu/latest/icu/datetime/struct.TimeFormatter.html Rust documentation for `TimeFormatter`} for more information.
 */
export class ICU4XTimeFormatter {

  /**

   * Creates a new {@link ICU4XTimeFormatter `ICU4XTimeFormatter`} from locale data.

   * See the {@link https://docs.rs/icu/latest/icu/datetime/struct.TimeFormatter.html#method.try_new_with_length_unstable Rust documentation for `try_new_with_length_unstable`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_with_length(provider: ICU4XDataProvider, locale: ICU4XLocale, length: ICU4XTimeLength): ICU4XTimeFormatter | never;

  /**

   * Formats a {@link ICU4XTime `ICU4XTime`} to a string.

   * See the {@link https://docs.rs/icu/latest/icu/datetime/struct.TimeFormatter.html#method.format Rust documentation for `format`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  format_time(value: ICU4XTime): string | never;

  /**

   * Formats a {@link ICU4XDateTime `ICU4XDateTime`} to a string.

   * See the {@link https://docs.rs/icu/latest/icu/datetime/struct.TimeFormatter.html#method.format Rust documentation for `format`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  format_datetime(value: ICU4XDateTime): string | never;

  /**

   * Formats a {@link ICU4XIsoDateTime `ICU4XIsoDateTime`} to a string.

   * See the {@link https://docs.rs/icu/latest/icu/datetime/struct.TimeFormatter.html#method.format Rust documentation for `format`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  format_iso_datetime(value: ICU4XIsoDateTime): string | never;
}
