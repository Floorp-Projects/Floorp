import { CodePointRangeIteratorResult } from "./CodePointRangeIteratorResult";

/**

 * An iterator over code point ranges, produced by `ICU4XCodePointSetData` or one of the `ICU4XCodePointMapData` types
 */
export class CodePointRangeIterator {

  /**

   * Advance the iterator by one and return the next range.

   * If the iterator is out of items, `done` will be true
   */
  next(): CodePointRangeIteratorResult;
}
