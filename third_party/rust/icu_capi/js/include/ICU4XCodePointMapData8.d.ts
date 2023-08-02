import { u8, u32, char } from "./diplomat-runtime"
import { FFIError } from "./diplomat-runtime"
import { CodePointRangeIterator } from "./CodePointRangeIterator";
import { ICU4XCodePointSetData } from "./ICU4XCodePointSetData";
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";

/**

 * An ICU4X Unicode Map Property object, capable of querying whether a code point (key) to obtain the Unicode property value, for a specific Unicode property.

 * For properties whose values fit into 8 bits.

 * See the {@link https://docs.rs/icu/latest/icu/properties/index.html Rust documentation for `properties`} for more information.

 * See the {@link https://docs.rs/icu/latest/icu/properties/maps/struct.CodePointMapData.html Rust documentation for `CodePointMapData`} for more information.

 * See the {@link https://docs.rs/icu/latest/icu/properties/maps/struct.CodePointMapDataBorrowed.html Rust documentation for `CodePointMapDataBorrowed`} for more information.
 */
export class ICU4XCodePointMapData8 {

  /**

   * Gets the value for a code point.

   * See the {@link https://docs.rs/icu/latest/icu/properties/maps/struct.CodePointMapDataBorrowed.html#method.get Rust documentation for `get`} for more information.
   */
  get(cp: char): u8;

  /**

   * Gets the value for a code point (specified as a 32 bit integer, in UTF-32)
   */
  get32(cp: u32): u8;

  /**

   * Converts a general category to its corresponding mask value

   * Nonexistant general categories will map to the empty mask

   * See the {@link https://docs.rs/icu/latest/icu/properties/struct.GeneralCategoryGroup.html Rust documentation for `GeneralCategoryGroup`} for more information.
   */
  static general_category_to_mask(gc: u8): u32;

  /**

   * Produces an iterator over ranges of code points that map to `value`

   * See the {@link https://docs.rs/icu/latest/icu/properties/maps/struct.CodePointMapDataBorrowed.html#method.iter_ranges_for_value Rust documentation for `iter_ranges_for_value`} for more information.
   */
  iter_ranges_for_value(value: u8): CodePointRangeIterator;

  /**

   * Produces an iterator over ranges of code points that do not map to `value`

   * See the {@link https://docs.rs/icu/latest/icu/properties/maps/struct.CodePointMapDataBorrowed.html#method.iter_ranges_for_value_complemented Rust documentation for `iter_ranges_for_value_complemented`} for more information.
   */
  iter_ranges_for_value_complemented(value: u8): CodePointRangeIterator;

  /**

   * Given a mask value (the nth bit marks property value = n), produce an iterator over ranges of code points whose property values are contained in the mask.

   * The main mask property supported is that for General_Category, which can be obtained via `general_category_to_mask()` or by using `ICU4XGeneralCategoryNameToMaskMapper`

   * Should only be used on maps for properties with values less than 32 (like Generak_Category), other maps will have unpredictable results

   * See the {@link https://docs.rs/icu/latest/icu/properties/maps/struct.CodePointMapDataBorrowed.html#method.iter_ranges_for_group Rust documentation for `iter_ranges_for_group`} for more information.
   */
  iter_ranges_for_mask(mask: u32): CodePointRangeIterator;

  /**

   * Gets a {@link ICU4XCodePointSetData `ICU4XCodePointSetData`} representing all entries in this map that map to the given value

   * See the {@link https://docs.rs/icu/latest/icu/properties/maps/struct.CodePointMapDataBorrowed.html#method.get_set_for_value Rust documentation for `get_set_for_value`} for more information.
   */
  get_set_for_value(value: u8): ICU4XCodePointSetData;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/maps/fn.load_general_category.html Rust documentation for `load_general_category`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_general_category(provider: ICU4XDataProvider): ICU4XCodePointMapData8 | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/maps/fn.load_bidi_class.html Rust documentation for `load_bidi_class`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_bidi_class(provider: ICU4XDataProvider): ICU4XCodePointMapData8 | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/maps/fn.load_east_asian_width.html Rust documentation for `load_east_asian_width`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_east_asian_width(provider: ICU4XDataProvider): ICU4XCodePointMapData8 | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/maps/fn.load_line_break.html Rust documentation for `load_line_break`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_line_break(provider: ICU4XDataProvider): ICU4XCodePointMapData8 | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/maps/fn.load_grapheme_cluster_break.html Rust documentation for `load_grapheme_cluster_break`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static try_grapheme_cluster_break(provider: ICU4XDataProvider): ICU4XCodePointMapData8 | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/maps/fn.load_word_break.html Rust documentation for `load_word_break`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_word_break(provider: ICU4XDataProvider): ICU4XCodePointMapData8 | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/maps/fn.load_sentence_break.html Rust documentation for `load_sentence_break`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_sentence_break(provider: ICU4XDataProvider): ICU4XCodePointMapData8 | never;
}
