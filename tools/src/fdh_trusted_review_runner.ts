// SPDX-License-Identifier: MPL-2.0

import {
  trustedReceiptFailure,
  validateTrustedReceiptCheck,
} from "./fdh_trusted_review.ts";
import type { TrustedCheckCliInput } from "./fdh_trusted_review_types.ts";

const inputText = await new Response(Deno.stdin.readable).text();
let input: unknown;
try {
  input = JSON.parse(inputText);
} catch {
  console.error("trusted receipt input is invalid JSON");
  Deno.exit(2);
}
const parsed = input as Partial<TrustedCheckCliInput>;
let result;
try {
  result = validateTrustedReceiptCheck(input as TrustedCheckCliInput);
} catch (error) {
  try {
    result = trustedReceiptFailure(parsed.expectedHeadSha ?? "", error);
  } catch {
    console.error("trusted receipt input has no valid expected head SHA");
    Deno.exit(2);
  }
}
console.log(JSON.stringify(result));
