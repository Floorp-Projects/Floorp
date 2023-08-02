import { i32 } from "./diplomat-runtime"

/**

 * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterBreakIterator.html Rust documentation for `GraphemeClusterBreakIterator`} for more information.
 */
export class ICU4XGraphemeClusterBreakIteratorUtf16 {

  /**

   * Finds the next breakpoint. Returns -1 if at the end of the string or if the index is out of range of a 32-bit signed integer.

   * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterBreakIterator.html#method.next Rust documentation for `next`} for more information.
   */
  next(): i32;
}
