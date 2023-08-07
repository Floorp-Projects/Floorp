import { FFIError } from "./diplomat-runtime"
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";
import { ICU4XLineBreakIteratorLatin1 } from "./ICU4XLineBreakIteratorLatin1";
import { ICU4XLineBreakIteratorUtf16 } from "./ICU4XLineBreakIteratorUtf16";
import { ICU4XLineBreakIteratorUtf8 } from "./ICU4XLineBreakIteratorUtf8";
import { ICU4XLineBreakOptionsV1 } from "./ICU4XLineBreakOptionsV1";

/**

 * An ICU4X line-break segmenter, capable of finding breakpoints in strings.

 * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.LineSegmenter.html Rust documentation for `LineSegmenter`} for more information.
 */
export class ICU4XLineSegmenter {

  /**

   * Construct a {@link ICU4XLineSegmenter `ICU4XLineSegmenter`} with default options. It automatically loads the best available payload data for Burmese, Khmer, Lao, and Thai.

   * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.LineSegmenter.html#method.try_new_auto_unstable Rust documentation for `try_new_auto_unstable`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_auto(provider: ICU4XDataProvider): ICU4XLineSegmenter | never;

  /**

   * Construct a {@link ICU4XLineSegmenter `ICU4XLineSegmenter`} with default options and LSTM payload data for Burmese, Khmer, Lao, and Thai.

   * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.LineSegmenter.html#method.try_new_lstm_unstable Rust documentation for `try_new_lstm_unstable`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_lstm(provider: ICU4XDataProvider): ICU4XLineSegmenter | never;

  /**

   * Construct a {@link ICU4XLineSegmenter `ICU4XLineSegmenter`} with default options and dictionary payload data for Burmese, Khmer, Lao, and Thai..

   * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.LineSegmenter.html#method.try_new_dictionary_unstable Rust documentation for `try_new_dictionary_unstable`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_dictionary(provider: ICU4XDataProvider): ICU4XLineSegmenter | never;

  /**

   * Construct a {@link ICU4XLineSegmenter `ICU4XLineSegmenter`} with custom options. It automatically loads the best available payload data for Burmese, Khmer, Lao, and Thai.

   * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.LineSegmenter.html#method.try_new_auto_with_options_unstable Rust documentation for `try_new_auto_with_options_unstable`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_auto_with_options_v1(provider: ICU4XDataProvider, options: ICU4XLineBreakOptionsV1): ICU4XLineSegmenter | never;

  /**

   * Construct a {@link ICU4XLineSegmenter `ICU4XLineSegmenter`} with custom options and LSTM payload data for Burmese, Khmer, Lao, and Thai.

   * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.LineSegmenter.html#method.try_new_lstm_with_options_unstable Rust documentation for `try_new_lstm_with_options_unstable`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_lstm_with_options_v1(provider: ICU4XDataProvider, options: ICU4XLineBreakOptionsV1): ICU4XLineSegmenter | never;

  /**

   * Construct a {@link ICU4XLineSegmenter `ICU4XLineSegmenter`} with custom options and dictionary payload data for Burmese, Khmer, Lao, and Thai.

   * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.LineSegmenter.html#method.try_new_dictionary_with_options_unstable Rust documentation for `try_new_dictionary_with_options_unstable`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_dictionary_with_options_v1(provider: ICU4XDataProvider, options: ICU4XLineBreakOptionsV1): ICU4XLineSegmenter | never;

  /**

   * Segments a (potentially ill-formed) UTF-8 string.

   * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.LineSegmenter.html#method.segment_utf8 Rust documentation for `segment_utf8`} for more information.
   */
  segment_utf8(input: string): ICU4XLineBreakIteratorUtf8;

  /**

   * Segments a UTF-16 string.

   * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.LineSegmenter.html#method.segment_utf16 Rust documentation for `segment_utf16`} for more information.
   */
  segment_utf16(input: Uint16Array): ICU4XLineBreakIteratorUtf16;

  /**

   * Segments a Latin-1 string.

   * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.LineSegmenter.html#method.segment_latin1 Rust documentation for `segment_latin1`} for more information.
   */
  segment_latin1(input: Uint8Array): ICU4XLineBreakIteratorLatin1;
}
