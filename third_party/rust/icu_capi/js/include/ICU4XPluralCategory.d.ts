import { FFIError } from "./diplomat-runtime"

/**

 * FFI version of `PluralCategory`.

 * See the {@link https://docs.rs/icu/latest/icu/plurals/enum.PluralCategory.html Rust documentation for `PluralCategory`} for more information.
 */
export enum ICU4XPluralCategory {
  /**
   */
  Zero = 'Zero',
  /**
   */
  One = 'One',
  /**
   */
  Two = 'Two',
  /**
   */
  Few = 'Few',
  /**
   */
  Many = 'Many',
  /**
   */
  Other = 'Other',
}