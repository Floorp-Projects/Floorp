import { FFIError } from "./diplomat-runtime"
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XDataStruct } from "./ICU4XDataStruct";
import { ICU4XError } from "./ICU4XError";
import { ICU4XFixedDecimal } from "./ICU4XFixedDecimal";
import { ICU4XFixedDecimalGroupingStrategy } from "./ICU4XFixedDecimalGroupingStrategy";
import { ICU4XLocale } from "./ICU4XLocale";

/**

 * An ICU4X Fixed Decimal Format object, capable of formatting a {@link ICU4XFixedDecimal `ICU4XFixedDecimal`} as a string.

 * See the {@link https://docs.rs/icu/latest/icu/decimal/struct.FixedDecimalFormatter.html Rust documentation for `FixedDecimalFormatter`} for more information.
 */
export class ICU4XFixedDecimalFormatter {

  /**

   * Creates a new {@link ICU4XFixedDecimalFormatter `ICU4XFixedDecimalFormatter`} from locale data.

   * See the {@link https://docs.rs/icu/latest/icu/decimal/struct.FixedDecimalFormatter.html#method.try_new_unstable Rust documentation for `try_new_unstable`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_with_grouping_strategy(provider: ICU4XDataProvider, locale: ICU4XLocale, grouping_strategy: ICU4XFixedDecimalGroupingStrategy): ICU4XFixedDecimalFormatter | never;

  /**

   * Creates a new {@link ICU4XFixedDecimalFormatter `ICU4XFixedDecimalFormatter`} from preconstructed locale data in the form of an {@link ICU4XDataStruct `ICU4XDataStruct`} constructed from `ICU4XDataStruct::create_decimal_symbols()`.

   * The contents of the data struct will be consumed: if you wish to use the struct again it will have to be reconstructed. Passing a consumed struct to this method will return an error.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_with_decimal_symbols_v1(data_struct: ICU4XDataStruct, grouping_strategy: ICU4XFixedDecimalGroupingStrategy): ICU4XFixedDecimalFormatter | never;

  /**

   * Formats a {@link ICU4XFixedDecimal `ICU4XFixedDecimal`} to a string.

   * See the {@link https://docs.rs/icu/latest/icu/decimal/struct.FixedDecimalFormatter.html#method.format Rust documentation for `format`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  format(value: ICU4XFixedDecimal): string | never;
}
