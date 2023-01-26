import { EMPTY_STRING } from "./constants";

/**
 * String preparation function. In a future here will be realization of algorithm from RFC4518
 * @param inputString JavaScript string. As soon as for each ASN.1 string type we have a specific
 *                    transformation function here we will work with pure JavaScript string
 * @returns Formatted string
 */
export function stringPrep(inputString: string): string {
  //#region Initial variables
  let isSpace = false;
  let cutResult = EMPTY_STRING;
  //#endregion

  const result = inputString.trim(); // Trim input string

  //#region Change all sequence of SPACE down to SPACE char
  for (let i = 0; i < result.length; i++) {
    if (result.charCodeAt(i) === 32) {
      if (isSpace === false)
        isSpace = true;
    } else {
      if (isSpace) {
        cutResult += " ";
        isSpace = false;
      }

      cutResult += result[i];
    }
  }
  //#endregion

  return cutResult.toLowerCase();
}
