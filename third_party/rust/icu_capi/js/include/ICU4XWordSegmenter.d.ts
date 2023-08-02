import { FFIError } from "./diplomat-runtime"
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";
import { ICU4XWordBreakIteratorLatin1 } from "./ICU4XWordBreakIteratorLatin1";
import { ICU4XWordBreakIteratorUtf16 } from "./ICU4XWordBreakIteratorUtf16";
import { ICU4XWordBreakIteratorUtf8 } from "./ICU4XWordBreakIteratorUtf8";

/**

 * An ICU4X word-break segmenter, capable of finding word breakpoints in strings.

 * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.WordSegmenter.html Rust documentation for `WordSegmenter`} for more information.
 */
export class ICU4XWordSegmenter {

  /**

   * Construct an {@link ICU4XWordSegmenter `ICU4XWordSegmenter`} with automatically selecting the best available LSTM or dictionary payload data.

   * Note: currently, it uses dictionary for Chinese and Japanese, and LSTM for Burmese, Khmer, Lao, and Thai.

   * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.WordSegmenter.html#method.try_new_auto_unstable Rust documentation for `try_new_auto_unstable`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_auto(provider: ICU4XDataProvider): ICU4XWordSegmenter | never;

  /**

   * Construct an {@link ICU4XWordSegmenter `ICU4XWordSegmenter`} with LSTM payload data for Burmese, Khmer, Lao, and Thai.

   * Warning: {@link ICU4XWordSegmenter `ICU4XWordSegmenter`} created by this function doesn't handle Chinese or Japanese.

   * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.WordSegmenter.html#method.try_new_lstm_unstable Rust documentation for `try_new_lstm_unstable`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_lstm(provider: ICU4XDataProvider): ICU4XWordSegmenter | never;

  /**

   * Construct an {@link ICU4XWordSegmenter `ICU4XWordSegmenter`} with dictionary payload data for Chinese, Japanese, Burmese, Khmer, Lao, and Thai.

   * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.WordSegmenter.html#method.try_new_dictionary_unstable Rust documentation for `try_new_dictionary_unstable`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_dictionary(provider: ICU4XDataProvider): ICU4XWordSegmenter | never;

  /**

   * Segments a (potentially ill-formed) UTF-8 string.

   * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.WordSegmenter.html#method.segment_utf8 Rust documentation for `segment_utf8`} for more information.
   */
  segment_utf8(input: string): ICU4XWordBreakIteratorUtf8;

  /**

   * Segments a UTF-16 string.

   * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.WordSegmenter.html#method.segment_utf16 Rust documentation for `segment_utf16`} for more information.
   */
  segment_utf16(input: Uint16Array): ICU4XWordBreakIteratorUtf16;

  /**

   * Segments a Latin-1 string.

   * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.WordSegmenter.html#method.segment_latin1 Rust documentation for `segment_latin1`} for more information.
   */
  segment_latin1(input: Uint8Array): ICU4XWordBreakIteratorLatin1;
}
