import { FFIError } from "./diplomat-runtime"
import { ICU4XError } from "./ICU4XError";
import { ICU4XLocale } from "./ICU4XLocale";

/**

 * The various calendar types currently supported by {@link ICU4XCalendar `ICU4XCalendar`}

 * See the {@link https://docs.rs/icu/latest/icu/calendar/enum.AnyCalendarKind.html Rust documentation for `AnyCalendarKind`} for more information.
 */
export enum ICU4XAnyCalendarKind {
  /**

   * The kind of an Iso calendar
   */
  Iso = 'Iso',
  /**

   * The kind of a Gregorian calendar
   */
  Gregorian = 'Gregorian',
  /**

   * The kind of a Buddhist calendar
   */
  Buddhist = 'Buddhist',
  /**

   * The kind of a Japanese calendar with modern eras
   */
  Japanese = 'Japanese',
  /**

   * The kind of a Japanese calendar with modern and historic eras
   */
  JapaneseExtended = 'JapaneseExtended',
  /**

   * The kind of an Ethiopian calendar, with Amete Mihret era
   */
  Ethiopian = 'Ethiopian',
  /**

   * The kind of an Ethiopian calendar, with Amete Alem era
   */
  EthiopianAmeteAlem = 'EthiopianAmeteAlem',
  /**

   * The kind of a Indian calendar
   */
  Indian = 'Indian',
  /**

   * The kind of a Coptic calendar
   */
  Coptic = 'Coptic',
}