import { ICU4XCollatorAlternateHandling } from "./ICU4XCollatorAlternateHandling";
import { ICU4XCollatorBackwardSecondLevel } from "./ICU4XCollatorBackwardSecondLevel";
import { ICU4XCollatorCaseFirst } from "./ICU4XCollatorCaseFirst";
import { ICU4XCollatorCaseLevel } from "./ICU4XCollatorCaseLevel";
import { ICU4XCollatorMaxVariable } from "./ICU4XCollatorMaxVariable";
import { ICU4XCollatorNumeric } from "./ICU4XCollatorNumeric";
import { ICU4XCollatorStrength } from "./ICU4XCollatorStrength";

/**

 * See the {@link https://docs.rs/icu/latest/icu/collator/struct.CollatorOptions.html Rust documentation for `CollatorOptions`} for more information.
 */
export class ICU4XCollatorOptionsV1 {
  strength: ICU4XCollatorStrength;
  alternate_handling: ICU4XCollatorAlternateHandling;
  case_first: ICU4XCollatorCaseFirst;
  max_variable: ICU4XCollatorMaxVariable;
  case_level: ICU4XCollatorCaseLevel;
  numeric: ICU4XCollatorNumeric;
  backward_second_level: ICU4XCollatorBackwardSecondLevel;
}
