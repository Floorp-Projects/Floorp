/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AddressResult", "CreditCardResult"];
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  CreditCard: "resource://gre/modules/CreditCard.jsm",
  FormAutofillUtils: "resource://autofill/FormAutofillUtils.jsm",
});

XPCOMUtils.defineLazyGetter(
  lazy,
  "l10n",
  () => new Localization(["browser/preferences/formAutofill.ftl"], true)
);

class ProfileAutoCompleteResult {
  constructor(
    searchString,
    focusedFieldName,
    allFieldNames,
    matchingProfiles,
    { resultCode = null, isSecure = true, isInputAutofilled = false }
  ) {
    // nsISupports
    this.QueryInterface = ChromeUtils.generateQI(["nsIAutoCompleteResult"]);

    // The user's query string
    this.searchString = searchString;
    // The field name of the focused input.
    this._focusedFieldName = focusedFieldName;
    // The matching profiles contains the information for filling forms.
    this._matchingProfiles = matchingProfiles;
    // The default item that should be entered if none is selected
    this.defaultIndex = 0;
    // The reason the search failed
    this.errorDescription = "";
    // The value used to determine whether the form is secure or not.
    this._isSecure = isSecure;
    // The value to indicate whether the focused input has been autofilled or not.
    this._isInputAutofilled = isInputAutofilled;
    // All fillable field names in the form including the field name of the currently-focused input.
    this._allFieldNames = [
      ...this._matchingProfiles.reduce((fieldSet, curProfile) => {
        for (let field of Object.keys(curProfile)) {
          fieldSet.add(field);
        }

        return fieldSet;
      }, new Set()),
    ].filter(field => allFieldNames.includes(field));

    // Force return success code if the focused field is auto-filled in order
    // to show clear form button popup.
    if (isInputAutofilled) {
      resultCode = Ci.nsIAutoCompleteResult.RESULT_SUCCESS;
    }
    // The result code of this result object.
    if (resultCode) {
      this.searchResult = resultCode;
    } else if (matchingProfiles.length) {
      this.searchResult = Ci.nsIAutoCompleteResult.RESULT_SUCCESS;
    } else {
      this.searchResult = Ci.nsIAutoCompleteResult.RESULT_NOMATCH;
    }

    // An array of primary and secondary labels for each profile.
    this._popupLabels = this._generateLabels(
      this._focusedFieldName,
      this._allFieldNames,
      this._matchingProfiles
    );
  }

  /**
   * @returns {number} The number of results
   */
  get matchCount() {
    return this._popupLabels.length;
  }

  _checkIndexBounds(index) {
    if (index < 0 || index >= this._popupLabels.length) {
      throw Components.Exception(
        "Index out of range.",
        Cr.NS_ERROR_ILLEGAL_VALUE
      );
    }
  }

  /**
   * Get the secondary label based on the focused field name and related field names
   * in the same form.
   * @param   {string} focusedFieldName The field name of the focused input
   * @param   {Array<Object>} allFieldNames The field names in the same section
   * @param   {object} profile The profile providing the labels to show.
   * @returns {string} The secondary label
   */
  _getSecondaryLabel(focusedFieldName, allFieldNames, profile) {
    return "";
  }

  _generateLabels(focusedFieldName, allFieldNames, profiles) {}

  /**
   * Get the value of the result at the given index.
   *
   * Always return empty string for form autofill feature to suppress
   * AutoCompleteController from autofilling, as we'll populate the
   * fields on our own.
   *
   * @param   {number} index The index of the result requested
   * @returns {string} The result at the specified index
   */
  getValueAt(index) {
    this._checkIndexBounds(index);
    return "";
  }

  getLabelAt(index) {
    this._checkIndexBounds(index);

    let label = this._popupLabels[index];
    if (typeof label == "string") {
      return label;
    }
    return JSON.stringify(label);
  }

  /**
   * Retrieves a comment (metadata instance)
   * @param   {number} index The index of the comment requested
   * @returns {string} The comment at the specified index
   */
  getCommentAt(index) {
    this._checkIndexBounds(index);
    return JSON.stringify(this._matchingProfiles[index]);
  }

  /**
   * Retrieves a style hint specific to a particular index.
   * @param   {number} index The index of the style hint requested
   * @returns {string} The style hint at the specified index
   */
  getStyleAt(index) {
    this._checkIndexBounds(index);
    if (index == this.matchCount - 1) {
      return "autofill-footer";
    }
    if (this._isInputAutofilled) {
      return "autofill-clear-button";
    }

    return "autofill-profile";
  }

  /**
   * Retrieves an image url.
   * @param   {number} index The index of the image url requested
   * @returns {string} The image url at the specified index
   */
  getImageAt(index) {
    this._checkIndexBounds(index);
    return "";
  }

  /**
   * Retrieves a result
   * @param   {number} index The index of the result requested
   * @returns {string} The result at the specified index
   */
  getFinalCompleteValueAt(index) {
    return this.getValueAt(index);
  }

  /**
   * Returns true if the value at the given index is removable
   * @param   {number}  index The index of the result to remove
   * @returns {boolean} True if the value is removable
   */
  isRemovableAt(index) {
    return true;
  }

  /**
   * Removes a result from the resultset
   * @param {number} index The index of the result to remove
   */
  removeValueAt(index) {
    // There is no plan to support removing profiles via autocomplete.
  }
}

class AddressResult extends ProfileAutoCompleteResult {
  constructor(...args) {
    super(...args);
  }

  _getSecondaryLabel(focusedFieldName, allFieldNames, profile) {
    // We group similar fields into the same field name so we won't pick another
    // field in the same group as the secondary label.
    const GROUP_FIELDS = {
      name: ["name", "given-name", "additional-name", "family-name"],
      "street-address": [
        "street-address",
        "address-line1",
        "address-line2",
        "address-line3",
      ],
      "country-name": ["country", "country-name"],
      tel: [
        "tel",
        "tel-country-code",
        "tel-national",
        "tel-area-code",
        "tel-local",
        "tel-local-prefix",
        "tel-local-suffix",
      ],
    };

    const secondaryLabelOrder = [
      "street-address", // Street address
      "name", // Full name
      "address-level3", // Townland / Neighborhood / Village
      "address-level2", // City/Town
      "organization", // Company or organization name
      "address-level1", // Province/State (Standardized code if possible)
      "country-name", // Country name
      "postal-code", // Postal code
      "tel", // Phone number
      "email", // Email address
    ];

    for (let field in GROUP_FIELDS) {
      if (GROUP_FIELDS[field].includes(focusedFieldName)) {
        focusedFieldName = field;
        break;
      }
    }

    for (const currentFieldName of secondaryLabelOrder) {
      if (focusedFieldName == currentFieldName || !profile[currentFieldName]) {
        continue;
      }

      let matching = GROUP_FIELDS[currentFieldName]
        ? allFieldNames.some(fieldName =>
            GROUP_FIELDS[currentFieldName].includes(fieldName)
          )
        : allFieldNames.includes(currentFieldName);

      if (matching) {
        if (
          currentFieldName == "street-address" &&
          profile["-moz-street-address-one-line"]
        ) {
          return profile["-moz-street-address-one-line"];
        }
        return profile[currentFieldName];
      }
    }

    return ""; // Nothing matched.
  }

  _generateLabels(focusedFieldName, allFieldNames, profiles) {
    if (this._isInputAutofilled) {
      return [
        { primary: "", secondary: "" }, // Clear button
        { primary: "", secondary: "" }, // Footer
      ];
    }

    // Skip results without a primary label.
    let labels = profiles
      .filter(profile => {
        return !!profile[focusedFieldName];
      })
      .map(profile => {
        let primaryLabel = profile[focusedFieldName];
        if (
          focusedFieldName == "street-address" &&
          profile["-moz-street-address-one-line"]
        ) {
          primaryLabel = profile["-moz-street-address-one-line"];
        }
        return {
          primary: primaryLabel,
          secondary: this._getSecondaryLabel(
            focusedFieldName,
            allFieldNames,
            profile
          ),
        };
      });
    // Add an empty result entry for footer. Its content will come from
    // the footer binding, so don't assign any value to it.
    // The additional properties: categories and focusedCategory are required of
    // the popup to generate autofill hint on the footer.
    labels.push({
      primary: "",
      secondary: "",
      categories: lazy.FormAutofillUtils.getCategoriesFromFieldNames(
        this._allFieldNames
      ),
      focusedCategory: lazy.FormAutofillUtils.getCategoryFromFieldName(
        this._focusedFieldName
      ),
    });

    return labels;
  }
}

class CreditCardResult extends ProfileAutoCompleteResult {
  constructor(...args) {
    super(...args);
    this._cardTypes = this._generateCardTypes(
      this._focusedFieldName,
      this._allFieldNames,
      this._matchingProfiles
    );
  }

  _getSecondaryLabel(focusedFieldName, allFieldNames, profile) {
    const GROUP_FIELDS = {
      "cc-name": [
        "cc-name",
        "cc-given-name",
        "cc-additional-name",
        "cc-family-name",
      ],
      "cc-exp": ["cc-exp", "cc-exp-month", "cc-exp-year"],
    };

    const secondaryLabelOrder = [
      "cc-number", // Credit card number
      "cc-name", // Full name
      "cc-exp", // Expiration date
    ];

    for (let field in GROUP_FIELDS) {
      if (GROUP_FIELDS[field].includes(focusedFieldName)) {
        focusedFieldName = field;
        break;
      }
    }

    for (const currentFieldName of secondaryLabelOrder) {
      if (focusedFieldName == currentFieldName || !profile[currentFieldName]) {
        continue;
      }

      let matching = GROUP_FIELDS[currentFieldName]
        ? allFieldNames.some(fieldName =>
            GROUP_FIELDS[currentFieldName].includes(fieldName)
          )
        : allFieldNames.includes(currentFieldName);

      if (matching) {
        if (currentFieldName == "cc-number") {
          let { affix, label } = lazy.CreditCard.formatMaskedNumber(
            profile[currentFieldName]
          );
          return affix + label;
        }
        return profile[currentFieldName];
      }
    }

    return ""; // Nothing matched.
  }

  _generateLabels(focusedFieldName, allFieldNames, profiles) {
    if (!this._isSecure) {
      let brandName = lazy.FormAutofillUtils.brandBundle.GetStringFromName(
        "brandShortName"
      );

      return [
        lazy.FormAutofillUtils.stringBundle.formatStringFromName(
          "insecureFieldWarningDescription",
          [brandName]
        ),
      ];
    }

    if (this._isInputAutofilled) {
      return [
        { primary: "", secondary: "" }, // Clear button
        { primary: "", secondary: "" }, // Footer
      ];
    }

    // Skip results without a primary label.
    let labels = profiles
      .filter(profile => {
        return !!profile[focusedFieldName];
      })
      .map(profile => {
        let primaryAffix;
        let primary = profile[focusedFieldName];

        if (focusedFieldName == "cc-number") {
          let { affix, label } = lazy.CreditCard.formatMaskedNumber(primary);
          primaryAffix = affix;
          primary = label;
        }
        const secondary = this._getSecondaryLabel(
          focusedFieldName,
          allFieldNames,
          profile
        );
        // The card type is displayed visually using an image. For a11y, we need
        // to expose it as text. We do this using aria-label. However,
        // aria-label overrides the text content, so we must include that also.
        const ccType = profile["cc-type"];
        const ccTypeL10nId = lazy.CreditCard.getNetworkL10nId(ccType);
        const ccTypeName = ccTypeL10nId
          ? lazy.l10n.formatValueSync(ccTypeL10nId)
          : ccType ?? ""; // Unknown card type
        const ariaLabel = [ccTypeName, primaryAffix, primary, secondary]
          .filter(chunk => !!chunk) // Exclude empty chunks.
          .join(" ");
        return {
          primaryAffix,
          primary,
          secondary,
          ariaLabel,
        };
      });
    // Add an empty result entry for footer.
    labels.push({ primary: "", secondary: "" });

    return labels;
  }

  // This method needs to return an array that parallels the
  // array returned by _generateLabels, above. As a consequence,
  // its logic follows very closely.
  _generateCardTypes(focusedFieldName, allFieldNames, profiles) {
    if (this._isInputAutofilled) {
      return [
        "", // Clear button
        "", // Footer
      ];
    }

    // Skip results without a primary label.
    let cardTypes = profiles
      .filter(profile => {
        return !!profile[focusedFieldName];
      })
      .map(profile => profile["cc-type"]);

    // Add an empty result entry for footer.
    cardTypes.push("");
    return cardTypes;
  }

  getStyleAt(index) {
    this._checkIndexBounds(index);
    if (!this._isSecure) {
      return "autofill-insecureWarning";
    }

    return super.getStyleAt(index);
  }

  getImageAt(index) {
    this._checkIndexBounds(index);
    let network = this._cardTypes[index];
    return lazy.CreditCard.getCreditCardLogo(network);
  }
}
