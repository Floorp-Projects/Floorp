import { u32, char } from "./diplomat-runtime"
import { FFIError } from "./diplomat-runtime"
import { CodePointRangeIterator } from "./CodePointRangeIterator";
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";

/**

 * An ICU4X Unicode Set Property object, capable of querying whether a code point is contained in a set based on a Unicode property.

 * See the {@link https://docs.rs/icu/latest/icu/properties/index.html Rust documentation for `properties`} for more information.

 * See the {@link https://docs.rs/icu/latest/icu/properties/sets/struct.CodePointSetData.html Rust documentation for `CodePointSetData`} for more information.

 * See the {@link https://docs.rs/icu/latest/icu/properties/sets/struct.CodePointSetDataBorrowed.html Rust documentation for `CodePointSetDataBorrowed`} for more information.
 */
export class ICU4XCodePointSetData {

  /**

   * Checks whether the code point is in the set.

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/struct.CodePointSetDataBorrowed.html#method.contains Rust documentation for `contains`} for more information.
   */
  contains(cp: char): boolean;

  /**

   * Checks whether the code point (specified as a 32 bit integer, in UTF-32) is in the set.
   */
  contains32(cp: u32): boolean;

  /**

   * Produces an iterator over ranges of code points contained in this set

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/struct.CodePointSetDataBorrowed.html#method.iter_ranges Rust documentation for `iter_ranges`} for more information.
   */
  iter_ranges(): CodePointRangeIterator;

  /**

   * Produces an iterator over ranges of code points not contained in this set

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/struct.CodePointSetDataBorrowed.html#method.iter_ranges_complemented Rust documentation for `iter_ranges_complemented`} for more information.
   */
  iter_ranges_complemented(): CodePointRangeIterator;

  /**

   * which is a mask with the same format as the `U_GC_XX_MASK` mask in ICU4C

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_for_general_category_group.html Rust documentation for `load_for_general_category_group`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_for_general_category_group(provider: ICU4XDataProvider, group: u32): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_ascii_hex_digit.html Rust documentation for `load_ascii_hex_digit`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_ascii_hex_digit(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_alnum.html Rust documentation for `load_alnum`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_alnum(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_alphabetic.html Rust documentation for `load_alphabetic`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_alphabetic(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_bidi_control.html Rust documentation for `load_bidi_control`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_bidi_control(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_bidi_mirrored.html Rust documentation for `load_bidi_mirrored`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_bidi_mirrored(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_blank.html Rust documentation for `load_blank`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_blank(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_cased.html Rust documentation for `load_cased`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_cased(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_case_ignorable.html Rust documentation for `load_case_ignorable`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_case_ignorable(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_full_composition_exclusion.html Rust documentation for `load_full_composition_exclusion`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_full_composition_exclusion(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_changes_when_casefolded.html Rust documentation for `load_changes_when_casefolded`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_changes_when_casefolded(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_changes_when_casemapped.html Rust documentation for `load_changes_when_casemapped`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_changes_when_casemapped(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_changes_when_nfkc_casefolded.html Rust documentation for `load_changes_when_nfkc_casefolded`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_changes_when_nfkc_casefolded(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_changes_when_lowercased.html Rust documentation for `load_changes_when_lowercased`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_changes_when_lowercased(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_changes_when_titlecased.html Rust documentation for `load_changes_when_titlecased`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_changes_when_titlecased(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_changes_when_uppercased.html Rust documentation for `load_changes_when_uppercased`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_changes_when_uppercased(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_dash.html Rust documentation for `load_dash`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_dash(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_deprecated.html Rust documentation for `load_deprecated`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_deprecated(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_default_ignorable_code_point.html Rust documentation for `load_default_ignorable_code_point`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_default_ignorable_code_point(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_diacritic.html Rust documentation for `load_diacritic`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_diacritic(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_emoji_modifier_base.html Rust documentation for `load_emoji_modifier_base`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_emoji_modifier_base(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_emoji_component.html Rust documentation for `load_emoji_component`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_emoji_component(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_emoji_modifier.html Rust documentation for `load_emoji_modifier`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_emoji_modifier(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_emoji.html Rust documentation for `load_emoji`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_emoji(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_emoji_presentation.html Rust documentation for `load_emoji_presentation`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_emoji_presentation(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_extender.html Rust documentation for `load_extender`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_extender(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_extended_pictographic.html Rust documentation for `load_extended_pictographic`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_extended_pictographic(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_graph.html Rust documentation for `load_graph`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_graph(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_grapheme_base.html Rust documentation for `load_grapheme_base`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_grapheme_base(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_grapheme_extend.html Rust documentation for `load_grapheme_extend`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_grapheme_extend(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_grapheme_link.html Rust documentation for `load_grapheme_link`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_grapheme_link(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_hex_digit.html Rust documentation for `load_hex_digit`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_hex_digit(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_hyphen.html Rust documentation for `load_hyphen`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_hyphen(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_id_continue.html Rust documentation for `load_id_continue`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_id_continue(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_ideographic.html Rust documentation for `load_ideographic`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_ideographic(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_id_start.html Rust documentation for `load_id_start`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_id_start(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_ids_binary_operator.html Rust documentation for `load_ids_binary_operator`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_ids_binary_operator(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_ids_trinary_operator.html Rust documentation for `load_ids_trinary_operator`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_ids_trinary_operator(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_join_control.html Rust documentation for `load_join_control`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_join_control(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_logical_order_exception.html Rust documentation for `load_logical_order_exception`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_logical_order_exception(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_lowercase.html Rust documentation for `load_lowercase`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_lowercase(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_math.html Rust documentation for `load_math`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_math(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_noncharacter_code_point.html Rust documentation for `load_noncharacter_code_point`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_noncharacter_code_point(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_nfc_inert.html Rust documentation for `load_nfc_inert`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_nfc_inert(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_nfd_inert.html Rust documentation for `load_nfd_inert`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_nfd_inert(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_nfkc_inert.html Rust documentation for `load_nfkc_inert`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_nfkc_inert(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_nfkd_inert.html Rust documentation for `load_nfkd_inert`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_nfkd_inert(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_pattern_syntax.html Rust documentation for `load_pattern_syntax`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_pattern_syntax(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_pattern_white_space.html Rust documentation for `load_pattern_white_space`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_pattern_white_space(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_prepended_concatenation_mark.html Rust documentation for `load_prepended_concatenation_mark`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_prepended_concatenation_mark(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_print.html Rust documentation for `load_print`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_print(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_quotation_mark.html Rust documentation for `load_quotation_mark`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_quotation_mark(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_radical.html Rust documentation for `load_radical`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_radical(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_regional_indicator.html Rust documentation for `load_regional_indicator`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_regional_indicator(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_soft_dotted.html Rust documentation for `load_soft_dotted`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_soft_dotted(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_segment_starter.html Rust documentation for `load_segment_starter`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_segment_starter(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_case_sensitive.html Rust documentation for `load_case_sensitive`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_case_sensitive(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_sentence_terminal.html Rust documentation for `load_sentence_terminal`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_sentence_terminal(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_terminal_punctuation.html Rust documentation for `load_terminal_punctuation`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_terminal_punctuation(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_unified_ideograph.html Rust documentation for `load_unified_ideograph`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_unified_ideograph(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_uppercase.html Rust documentation for `load_uppercase`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_uppercase(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_variation_selector.html Rust documentation for `load_variation_selector`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_variation_selector(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_white_space.html Rust documentation for `load_white_space`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_white_space(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_xdigit.html Rust documentation for `load_xdigit`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_xdigit(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_xid_continue.html Rust documentation for `load_xid_continue`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_xid_continue(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_xid_start.html Rust documentation for `load_xid_start`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_xid_start(provider: ICU4XDataProvider): ICU4XCodePointSetData | never;

  /**

   * Loads data for a property specified as a string as long as it is one of the {@link https://tc39.es/ecma262/#table-binary-unicode-properties ECMA-262 binary properties} (not including Any, ASCII, and Assigned pseudoproperties).

   * Returns `ICU4XError::PropertyUnexpectedPropertyNameError` in case the string does not match any property in the list

   * See the {@link https://docs.rs/icu/latest/icu/properties/sets/fn.load_for_ecma262_unstable.html Rust documentation for `load_for_ecma262_unstable`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load_for_ecma262(provider: ICU4XDataProvider, property_name: string): ICU4XCodePointSetData | never;
}
