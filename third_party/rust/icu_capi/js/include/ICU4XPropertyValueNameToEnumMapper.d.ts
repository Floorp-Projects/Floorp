import { i16 } from "./diplomat-runtime"
import { FFIError } from "./diplomat-runtime"
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";

/**

 * A type capable of looking up a property value from a string name.

 * See the {@link https://docs.rs/icu/latest/icu/properties/names/struct.PropertyValueNameToEnumMapper.html Rust documentation for `PropertyValueNameToEnumMapper`} for more information.

 * See the {@link https://docs.rs/icu/latest/icu/properties/names/struct.PropertyValueNameToEnumMapperBorrowed.html Rust documentation for `PropertyValueNameToEnumMapperBorrowed`} for more information.
 */
export class ICU4XPropertyValueNameToEnumMapper {

  /**

   * Get the property value matching the given name, using strict matching

   * Returns -1 if the name is unknown for this property

   * See the {@link https://docs.rs/icu/latest/icu/properties/names/struct.PropertyValueNameToEnumMapperBorrowed.html#method.get_strict Rust documentation for `get_strict`} for more information.
   */
  get_strict(name: string): i16;

  /**

   * Get the property value matching the given name, using loose matching

   * Returns -1 if the name is unknown for this property

   * See the {@link https://docs.rs/icu/latest/icu/properties/names/struct.PropertyValueNameToEnumMapperBorrowed.html#method.get_loose Rust documentation for `get_loose`} for more information.
   */
  get_loose(name: string): i16;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/struct.GeneralCategory.html#method.get_name_to_enum_mapper Rust documentation for `get_name_to_enum_mapper`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_general_category(provider: ICU4XDataProvider): ICU4XPropertyValueNameToEnumMapper | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/struct.BidiClass.html#method.get_name_to_enum_mapper Rust documentation for `get_name_to_enum_mapper`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_bidi_class(provider: ICU4XDataProvider): ICU4XPropertyValueNameToEnumMapper | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/struct.EastAsianWidth.html#method.get_name_to_enum_mapper Rust documentation for `get_name_to_enum_mapper`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_east_asian_width(provider: ICU4XDataProvider): ICU4XPropertyValueNameToEnumMapper | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/struct.LineBreak.html#method.get_name_to_enum_mapper Rust documentation for `get_name_to_enum_mapper`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_line_break(provider: ICU4XDataProvider): ICU4XPropertyValueNameToEnumMapper | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/struct.GraphemeClusterBreak.html#method.get_name_to_enum_mapper Rust documentation for `get_name_to_enum_mapper`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_grapheme_cluster_break(provider: ICU4XDataProvider): ICU4XPropertyValueNameToEnumMapper | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/struct.WordBreak.html#method.get_name_to_enum_mapper Rust documentation for `get_name_to_enum_mapper`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_word_break(provider: ICU4XDataProvider): ICU4XPropertyValueNameToEnumMapper | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/struct.SentenceBreak.html#method.get_name_to_enum_mapper Rust documentation for `get_name_to_enum_mapper`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_sentence_break(provider: ICU4XDataProvider): ICU4XPropertyValueNameToEnumMapper | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/struct.Script.html#method.get_name_to_enum_mapper Rust documentation for `get_name_to_enum_mapper`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_script(provider: ICU4XDataProvider): ICU4XPropertyValueNameToEnumMapper | never;
}
