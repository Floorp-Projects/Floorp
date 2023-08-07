import { u8 } from "./diplomat-runtime"
import { FFIError } from "./diplomat-runtime"
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";
import { ICU4XIsoWeekday } from "./ICU4XIsoWeekday";
import { ICU4XLocale } from "./ICU4XLocale";

/**

 * A Week calculator, useful to be passed in to `week_of_year()` on Date and DateTime types

 * See the {@link https://docs.rs/icu/latest/icu/calendar/week/struct.WeekCalculator.html Rust documentation for `WeekCalculator`} for more information.
 */
export class ICU4XWeekCalculator {

  /**

   * Creates a new {@link ICU4XWeekCalculator `ICU4XWeekCalculator`} from locale data.

   * See the {@link https://docs.rs/icu/latest/icu/calendar/week/struct.WeekCalculator.html#method.try_new_unstable Rust documentation for `try_new_unstable`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create(provider: ICU4XDataProvider, locale: ICU4XLocale): ICU4XWeekCalculator | never;

  /**

   * Additional information: {@link https://docs.rs/icu/latest/icu/calendar/week/struct.WeekCalculator.html#structfield.first_weekday 1}, {@link https://docs.rs/icu/latest/icu/calendar/week/struct.WeekCalculator.html#structfield.min_week_days 2}
   */
  static create_from_first_day_of_week_and_min_week_days(first_weekday: ICU4XIsoWeekday, min_week_days: u8): ICU4XWeekCalculator;

  /**

   * Returns the weekday that starts the week for this object's locale

   * See the {@link https://docs.rs/icu/latest/icu/calendar/week/struct.WeekCalculator.html#structfield.first_weekday Rust documentation for `first_weekday`} for more information.
   */
  first_weekday(): ICU4XIsoWeekday;

  /**

   * The minimum number of days overlapping a year required for a week to be considered part of that year

   * See the {@link https://docs.rs/icu/latest/icu/calendar/week/struct.WeekCalculator.html#structfield.min_week_days Rust documentation for `min_week_days`} for more information.
   */
  min_week_days(): u8;
}
