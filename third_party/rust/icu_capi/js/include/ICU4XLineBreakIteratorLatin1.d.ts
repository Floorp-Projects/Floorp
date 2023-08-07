import { i32 } from "./diplomat-runtime"

/**

 * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.LineBreakIterator.html Rust documentation for `LineBreakIterator`} for more information.

 * Additional information: {@link https://docs.rs/icu/latest/icu/segmenter/type.LineBreakIteratorLatin1.html 1}
 */
export class ICU4XLineBreakIteratorLatin1 {

  /**

   * Finds the next breakpoint. Returns -1 if at the end of the string or if the index is out of range of a 32-bit signed integer.

   * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.LineBreakIterator.html#method.next Rust documentation for `next`} for more information.
   */
  next(): i32;
}
