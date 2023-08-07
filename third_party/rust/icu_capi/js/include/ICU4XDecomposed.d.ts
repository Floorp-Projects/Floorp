import { char } from "./diplomat-runtime"

/**

 * The outcome of non-recursive canonical decomposition of a character. `second` will be NUL when the decomposition expands to a single character (which may or may not be the original one)

 * See the {@link https://docs.rs/icu/latest/icu/normalizer/properties/enum.Decomposed.html Rust documentation for `Decomposed`} for more information.
 */
export class ICU4XDecomposed {
  first: char;
  second: char;
}
