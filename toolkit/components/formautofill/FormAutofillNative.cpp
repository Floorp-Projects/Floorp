/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FormAutofillNative.h"

#include <math.h>

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ComputedStyle.h"
#include "mozilla/dom/AutocompleteInfoBinding.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/HTMLLabelElement.h"
#include "mozilla/dom/HTMLOptionElement.h"
#include "mozilla/dom/HTMLSelectElement.h"
#include "mozilla/HashTable.h"
#include "mozilla/regex_ffi_generated.h"
#include "nsContentUtils.h"
#include "nsIFrame.h"
#include "nsIFrameInlines.h"
#include "nsLayoutUtils.h"
#include "nsTStringHasher.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {
template <>
class DefaultDelete<regex::ffi::RegexWrapper> {
 public:
  void operator()(regex::ffi::RegexWrapper* aPtr) const {
    regex::ffi::regex_delete(aPtr);
  }
};
}  // namespace mozilla

namespace mozilla::dom {

static const char kWhitespace[] = "\b\t\r\n ";

enum class RegexKey : uint8_t {
  CC_NAME,
  CC_NUMBER,
  CC_EXP,
  CC_EXP_MONTH,
  CC_EXP_YEAR,
  CC_TYPE,
  MM_MONTH,
  YY_OR_YYYY,
  MONTH,
  YEAR,
  MMYY,
  VISA_CHECKOUT,
  CREDIT_CARD_NETWORK,
  CREDIT_CARD_NETWORK_EXACT_MATCH,
  CREDIT_CARD_NETWORK_LONG,
  TWO_OR_FOUR_DIGIT_YEAR,
  DWFRM,
  BML,
  TEMPLATED_VALUE,
  FIRST,
  LAST,
  GIFT,
  SUBSCRIPTION,
  VALIDATION,

  Count
};

// We don't follow the coding style (naming start with capital letter) here and
// the following CCXXX enum class because we want to sync the rule naming with
// the JS implementation.
enum class CCNumberParams : uint8_t {
  idOrNameMatchNumberRegExp,
  labelsMatchNumberRegExp,
  placeholderMatchesNumberRegExp,
  ariaLabelMatchesNumberRegExp,
  idOrNameMatchGift,
  labelsMatchGift,
  placeholderMatchesGift,
  ariaLabelMatchesGift,
  idOrNameMatchSubscription,
  idOrNameMatchDwfrmAndBml,
  hasTemplatedValue,
  isNotVisible,
  inputTypeNotNumbery,

  Count,
};

enum class CCNameParams : uint8_t {
  idOrNameMatchNameRegExp,
  labelsMatchNameRegExp,
  placeholderMatchesNameRegExp,
  ariaLabelMatchesNameRegExp,
  idOrNameMatchFirst,
  labelsMatchFirst,
  placeholderMatchesFirst,
  ariaLabelMatchesFirst,
  idOrNameMatchLast,
  labelsMatchLast,
  placeholderMatchesLast,
  ariaLabelMatchesLast,
  idOrNameMatchFirstAndLast,
  idOrNameMatchSubscription,
  idOrNameMatchDwfrmAndBml,
  hasTemplatedValue,
  isNotVisible,

  Count,
};

enum class CCTypeParams : uint8_t {
  idOrNameMatchTypeRegExp,
  labelsMatchTypeRegExp,
  idOrNameMatchVisaCheckout,
  ariaLabelMatchesVisaCheckout,
  isSelectWithCreditCardOptions,
  isRadioWithCreditCardText,
  idOrNameMatchSubscription,
  idOrNameMatchDwfrmAndBml,
  hasTemplatedValue,

  Count,
};

enum class CCExpParams : uint8_t {
  labelsMatchExpRegExp,
  placeholderMatchesExpRegExp,
  labelsMatchExpWith2Or4DigitYear,
  placeholderMatchesExpWith2Or4DigitYear,
  labelsMatchMMYY,
  placeholderMatchesMMYY,
  maxLengthIs7,
  idOrNameMatchSubscription,
  idOrNameMatchDwfrmAndBml,
  hasTemplatedValue,
  isExpirationMonthLikely,
  isExpirationYearLikely,
  idOrNameMatchMonth,
  idOrNameMatchYear,
  idOrNameMatchExpMonthRegExp,
  idOrNameMatchExpYearRegExp,
  idOrNameMatchValidation,
  Count,
};

enum class CCExpMonthParams : uint8_t {
  idOrNameMatchExpMonthRegExp,
  labelsMatchExpMonthRegExp,
  placeholderMatchesExpMonthRegExp,
  ariaLabelMatchesExpMonthRegExp,
  idOrNameMatchMonth,
  labelsMatchMonth,
  placeholderMatchesMonth,
  ariaLabelMatchesMonth,
  nextFieldIdOrNameMatchExpYearRegExp,
  nextFieldLabelsMatchExpYearRegExp,
  nextFieldPlaceholderMatchExpYearRegExp,
  nextFieldAriaLabelMatchExpYearRegExp,
  nextFieldIdOrNameMatchYear,
  nextFieldLabelsMatchYear,
  nextFieldPlaceholderMatchesYear,
  nextFieldAriaLabelMatchesYear,
  nextFieldMatchesExpYearAutocomplete,
  isExpirationMonthLikely,
  nextFieldIsExpirationYearLikely,
  maxLengthIs2,
  placeholderMatchesMM,
  roleIsMenu,
  idOrNameMatchSubscription,
  idOrNameMatchDwfrmAndBml,
  hasTemplatedValue,

  Count,
};

enum class CCExpYearParams : uint8_t {
  idOrNameMatchExpYearRegExp,
  labelsMatchExpYearRegExp,
  placeholderMatchesExpYearRegExp,
  ariaLabelMatchesExpYearRegExp,
  idOrNameMatchYear,
  labelsMatchYear,
  placeholderMatchesYear,
  ariaLabelMatchesYear,
  previousFieldIdOrNameMatchExpMonthRegExp,
  previousFieldLabelsMatchExpMonthRegExp,
  previousFieldPlaceholderMatchExpMonthRegExp,
  previousFieldAriaLabelMatchExpMonthRegExp,
  previousFieldIdOrNameMatchMonth,
  previousFieldLabelsMatchMonth,
  previousFieldPlaceholderMatchesMonth,
  previousFieldAriaLabelMatchesMonth,
  previousFieldMatchesExpMonthAutocomplete,
  isExpirationYearLikely,
  previousFieldIsExpirationMonthLikely,
  placeholderMatchesYYOrYYYY,
  roleIsMenu,
  idOrNameMatchSubscription,
  idOrNameMatchDwfrmAndBml,
  hasTemplatedValue,

  Count,
};

struct AutofillParams {
  EnumeratedArray<CCNumberParams, CCNumberParams::Count, double>
      mCCNumberParams;
  EnumeratedArray<CCNameParams, CCNameParams::Count, double> mCCNameParams;
  EnumeratedArray<CCTypeParams, CCTypeParams::Count, double> mCCTypeParams;
  EnumeratedArray<CCExpParams, CCExpParams::Count, double> mCCExpParams;
  EnumeratedArray<CCExpMonthParams, CCExpMonthParams::Count, double>
      mCCExpMonthParams;
  EnumeratedArray<CCExpYearParams, CCExpYearParams::Count, double>
      mCCExpYearParams;
};

constexpr AutofillParams kCoefficents{
    .mCCNumberParams = {
        /* idOrNameMatchNumberRegExp */ 6.3039140701293945,
        /* labelsMatchNumberRegExp */ 2.8916432857513428,
        /* placeholderMatchesNumberRegExp */ 5.505742073059082,
        /* ariaLabelMatchesNumberRegExp */ 4.561432361602783,
        /* idOrNameMatchGift */ -3.803224563598633,
        /* labelsMatchGift */ -4.524861812591553,
        /* placeholderMatchesGift */ -1.8712525367736816,
        /* ariaLabelMatchesGift */ -3.674055576324463,
        /* idOrNameMatchSubscription */ -0.7810876369476318,
        /* idOrNameMatchDwfrmAndBml */ -1.095906138420105,
        /* hasTemplatedValue */ -6.368256568908691,
        /* isNotVisible */ -3.0330028533935547,
        /* inputTypeNotNumbery */ -2.300889253616333,
    },
    .mCCNameParams = {
        /* idOrNameMatchNameRegExp */ 7.953818321228027,
        /* labelsMatchNameRegExp */ 11.784907341003418,
        /* placeholderMatchesNameRegExp */ 9.202799797058105,
        /* ariaLabelMatchesNameRegExp */ 9.627416610717773,
        /* idOrNameMatchFirst */ -6.200107574462891,
        /* labelsMatchFirst */ -14.77401065826416,
        /* placeholderMatchesFirst */ -10.258772850036621,
        /* ariaLabelMatchesFirst */ -2.1574606895446777,
        /* idOrNameMatchLast */ -5.508854389190674,
        /* labelsMatchLast */ -14.563374519348145,
        /* placeholderMatchesLast */ -8.281961441040039,
        /* ariaLabelMatchesLast */ -7.915995121002197,
        /* idOrNameMatchFirstAndLast */ 17.586633682250977,
        /* idOrNameMatchSubscription */ -1.3862149715423584,
        /* idOrNameMatchDwfrmAndBml */ -1.154863953590393,
        /* hasTemplatedValue */ -1.3476886749267578,
        /* isNotVisible */ -20.619457244873047,
    },
    .mCCTypeParams = {
        /* idOrNameMatchTypeRegExp */ 2.815537691116333,
        /* labelsMatchTypeRegExp */ -2.6969387531280518,
        /* idOrNameMatchVisaCheckout */ -4.888851165771484,
        /* ariaLabelMatchesVisaCheckout */ -5.0021514892578125,
        /* isSelectWithCreditCardOptions */ 7.633410453796387,
        /* isRadioWithCreditCardText */ 9.72647762298584,
        /* idOrNameMatchSubscription */ -2.540968179702759,
        /* idOrNameMatchDwfrmAndBml */ -2.4342823028564453,
        /* hasTemplatedValue */ -2.134981155395508,
    },
    .mCCExpParams = {
        /* labelsMatchExpRegExp */ 7.235990524291992,
        /* placeholderMatchesExpRegExp */ 3.7828152179718018,
        /* labelsMatchExpWith2Or4DigitYear */ 3.28702449798584,
        /* placeholderMatchesExpWith2Or4DigitYear */ 0.9417413473129272,
        /* labelsMatchMMYY */ 8.527382850646973,
        /* placeholderMatchesMMYY */ 6.976727485656738,
        /* maxLengthIs7 */ -1.6640985012054443,
        /* idOrNameMatchSubscription */ -1.7390238046646118,
        /* idOrNameMatchDwfrmAndBml */ -1.8697377443313599,
        /* hasTemplatedValue */ -2.2890148162841797,
        /* isExpirationMonthLikely */ -2.7287368774414062,
        /* isExpirationYearLikely */ -2.1379034519195557,
        /* idOrNameMatchMonth */ -2.9298980236053467,
        /* idOrNameMatchYear */ -2.423668622970581,
        /* idOrNameMatchExpMonthRegExp */ -2.224165916442871,
        /* idOrNameMatchExpYearRegExp */ -2.4124796390533447,
        /* idOrNameMatchValidation */ -6.64445686340332,
    },
    .mCCExpMonthParams = {
        /* idOrNameMatchExpMonthRegExp */ 3.1759495735168457,
        /* labelsMatchExpMonthRegExp */ 0.6333072781562805,
        /* placeholderMatchesExpMonthRegExp */ -1.0211261510849,
        /* ariaLabelMatchesExpMonthRegExp */ -0.12013287842273712,
        /* idOrNameMatchMonth */ 0.8069844245910645,
        /* labelsMatchMonth */ 2.8041117191314697,
        /* placeholderMatchesMonth */ -0.7963107228279114,
        /* ariaLabelMatchesMonth */ -0.18894313275814056,
        /* nextFieldIdOrNameMatchExpYearRegExp */ 1.3703272342681885,
        /* nextFieldLabelsMatchExpYearRegExp */ 0.4734393060207367,
        /* nextFieldPlaceholderMatchExpYearRegExp */ -0.9648597240447998,
        /* nextFieldAriaLabelMatchExpYearRegExp */ 2.3334436416625977,
        /* nextFieldIdOrNameMatchYear */ 0.7225953936576843,
        /* nextFieldLabelsMatchYear */ 0.47795572876930237,
        /* nextFieldPlaceholderMatchesYear */ -1.032015085220337,
        /* nextFieldAriaLabelMatchesYear */ 2.5017199516296387,
        /* nextFieldMatchesExpYearAutocomplete */ 1.4952502250671387,
        /* isExpirationMonthLikely */ 5.659104347229004,
        /* nextFieldIsExpirationYearLikely */ 2.5078020095825195,
        /* maxLengthIs2 */ -0.5410940051078796,
        /* placeholderMatchesMM */ 7.3071208000183105,
        /* roleIsMenu */ 5.595693111419678,
        /* idOrNameMatchSubscription */ -5.626739978790283,
        /* idOrNameMatchDwfrmAndBml */ -7.236949920654297,
        /* hasTemplatedValue */ -6.055515289306641,
    },
    .mCCExpYearParams = {
        /* idOrNameMatchExpYearRegExp */ 2.456799268722534,
        /* labelsMatchExpYearRegExp */ 0.9488120675086975,
        /* placeholderMatchesExpYearRegExp */ -0.6318328380584717,
        /* ariaLabelMatchesExpYearRegExp */ -0.16433487832546234,
        /* idOrNameMatchYear */ 2.0227997303009033,
        /* labelsMatchYear */ 0.7777050733566284,
        /* placeholderMatchesYear */ -0.6191908121109009,
        /* ariaLabelMatchesYear */ -0.5337049961090088,
        /* previousFieldIdOrNameMatchExpMonthRegExp */ 0.2529127597808838,
        /* previousFieldLabelsMatchExpMonthRegExp */ 0.5853790044784546,
        /* previousFieldPlaceholderMatchExpMonthRegExp */ -0.710956871509552,
        /* previousFieldAriaLabelMatchExpMonthRegExp */ 2.2874839305877686,
        /* previousFieldIdOrNameMatchMonth */ -1.99709153175354,
        /* previousFieldLabelsMatchMonth */ 1.114603042602539,
        /* previousFieldPlaceholderMatchesMonth */ -0.4987318515777588,
        /* previousFieldAriaLabelMatchesMonth */ 2.1683783531188965,
        /* previousFieldMatchesExpMonthAutocomplete */ 1.2016327381134033,
        /* isExpirationYearLikely */ 5.7863616943359375,
        /* previousFieldIsExpirationMonthLikely */ 6.4013848304748535,
        /* placeholderMatchesYYOrYYYY */ 8.81661605834961,
        /* roleIsMenu */ 3.7794034481048584,
        /* idOrNameMatchSubscription */ -4.7467498779296875,
        /* idOrNameMatchDwfrmAndBml */ -5.523425102233887,
        /* hasTemplatedValue */ -6.14529275894165,
    }};

constexpr float kCCNumberBias = -4.422344207763672;
constexpr float kCCNameBias = -5.876968860626221;
constexpr float kCCTypeBias = -5.410860061645508;
constexpr float kCCExpBias = -5.439330577850342;
constexpr float kCCExpMonthBias = -5.99984073638916;
constexpr float kCCExpYearBias = -6.0192646980285645;

struct Rule {
  RegexKey key;
  const char* pattern;
};

const Rule kFirefoxRules[] = {
    {RegexKey::CC_NAME, "account.*holder.*name"},
    {RegexKey::CC_NUMBER, "(cc|kk)nr"},        // de-DE
    {RegexKey::CC_EXP_MONTH, "(cc|kk)month"},  // de-DE
    {RegexKey::CC_EXP_YEAR, "(cc|kk)year"},    // de-DE
    {RegexKey::CC_TYPE, "type"},

    {RegexKey::MM_MONTH, "^mm$|\\(mm\\)"},
    {RegexKey::YY_OR_YYYY, "^(yy|yyyy)$|\\(yy\\)|\\(yyyy\\)"},
    {RegexKey::MONTH, "month"},
    {RegexKey::YEAR, "year"},
    {RegexKey::MMYY, "mm\\s*(/|\\\\)\\s*yy"},
    {RegexKey::VISA_CHECKOUT, "visa(-|\\s)checkout"},
    // This should be a union of NETWORK_NAMES in CreditCard.jsm
    {RegexKey::CREDIT_CARD_NETWORK_LONG,
     "american express|master card|union pay"},
    // Please also update CREDIT_CARD_NETWORK_EXACT_MATCH while updating
    // CREDIT_CARD_NETWORK
    {RegexKey::CREDIT_CARD_NETWORK,
     "amex|cartebancaire|diners|discover|jcb|mastercard|mir|unionpay|visa"},
    {RegexKey::CREDIT_CARD_NETWORK_EXACT_MATCH,
     "^\\s*(?:amex|cartebancaire|diners|discover|jcb|mastercard|mir|unionpay|"
     "visa)\\s*$"},
    {RegexKey::TWO_OR_FOUR_DIGIT_YEAR,
     "(?:exp.*date[^y\\\\n\\\\r]*|mm\\\\s*[-/]?\\\\s*)yy(?:yy)?(?:[^y]|$)"},
    {RegexKey::DWFRM, "^dwfrm"},
    {RegexKey::BML, "BML"},
    {RegexKey::TEMPLATED_VALUE, "^\\{\\{.*\\}\\}$"},
    {RegexKey::FIRST, "first"},
    {RegexKey::LAST, "last"},
    {RegexKey::GIFT, "gift"},
    {RegexKey::SUBSCRIPTION, "subscription"},
    {RegexKey::VALIDATION, "validate|validation"},
};

// These are the rules used by Bitwarden [0], converted into RegExp form.
// [0]
// https://github.com/bitwarden/browser/blob/c2b8802201fac5e292d55d5caf3f1f78088d823c/src/services/autofill.service.ts#L436
const Rule kBitwardenRules[] = {
    /* eslint-disable */
    // Let us keep our consistent wrapping.
    {RegexKey::CC_NAME,
     "cc-?name"
     "|card-?name"
     "|cardholder-?name"
     "|(^nom$)"},
    /* eslint-enable */

    {RegexKey::CC_NUMBER,
     "cc-?number"
     "|cc-?num"
     "|card-?number"
     "|card-?num"
     "|cc-?no"
     "|card-?no"
     "|numero-?carte"
     "|num-?carte"
     "|cb-?num"},

    {RegexKey::CC_EXP,
     "(^cc-?exp$)"
     "|(^card-?exp$)"
     "|(^cc-?expiration$)"
     "|(^card-?expiration$)"
     "|(^cc-?ex$)"
     "|(^card-?ex$)"
     "|(^card-?expire$)"
     "|(^card-?expiry$)"
     "|(^validite$)"
     "|(^expiration$)"
     "|(^expiry$)"
     "|mm-?yy"
     "|mm-?yyyy"
     "|yy-?mm"
     "|yyyy-?mm"
     "|expiration-?date"
     "|payment-?card-?expiration"
     "|(^payment-?cc-?date$)"},

    {RegexKey::CC_EXP_MONTH,
     "(^exp-?month$)"
     "|(^cc-?exp-?month$)"
     "|(^cc-?month$)"
     "|(^card-?month$)"
     "|(^cc-?mo$)"
     "|(^card-?mo$)"
     "|(^exp-?mo$)"
     "|(^card-?exp-?mo$)"
     "|(^cc-?exp-?mo$)"
     "|(^card-?expiration-?month$)"
     "|(^expiration-?month$)"
     "|(^cc-?mm$)"
     "|(^cc-?m$)"
     "|(^card-?mm$)"
     "|(^card-?m$)"
     "|(^card-?exp-?mm$)"
     "|(^cc-?exp-?mm$)"
     "|(^exp-?mm$)"
     "|(^exp-?m$)"
     "|(^expire-?month$)"
     "|(^expire-?mo$)"
     "|(^expiry-?month$)"
     "|(^expiry-?mo$)"
     "|(^card-?expire-?month$)"
     "|(^card-?expire-?mo$)"
     "|(^card-?expiry-?month$)"
     "|(^card-?expiry-?mo$)"
     "|(^mois-?validite$)"
     "|(^mois-?expiration$)"
     "|(^m-?validite$)"
     "|(^m-?expiration$)"
     "|(^expiry-?date-?field-?month$)"
     "|(^expiration-?date-?month$)"
     "|(^expiration-?date-?mm$)"
     "|(^exp-?mon$)"
     "|(^validity-?mo$)"
     "|(^exp-?date-?mo$)"
     "|(^cb-?date-?mois$)"
     "|(^date-?m$)"},

    {RegexKey::CC_EXP_YEAR,
     "(^exp-?year$)"
     "|(^cc-?exp-?year$)"
     "|(^cc-?year$)"
     "|(^card-?year$)"
     "|(^cc-?yr$)"
     "|(^card-?yr$)"
     "|(^exp-?yr$)"
     "|(^card-?exp-?yr$)"
     "|(^cc-?exp-?yr$)"
     "|(^card-?expiration-?year$)"
     "|(^expiration-?year$)"
     "|(^cc-?yy$)"
     "|(^cc-?y$)"
     "|(^card-?yy$)"
     "|(^card-?y$)"
     "|(^card-?exp-?yy$)"
     "|(^cc-?exp-?yy$)"
     "|(^exp-?yy$)"
     "|(^exp-?y$)"
     "|(^cc-?yyyy$)"
     "|(^card-?yyyy$)"
     "|(^card-?exp-?yyyy$)"
     "|(^cc-?exp-?yyyy$)"
     "|(^expire-?year$)"
     "|(^expire-?yr$)"
     "|(^expiry-?year$)"
     "|(^expiry-?yr$)"
     "|(^card-?expire-?year$)"
     "|(^card-?expire-?yr$)"
     "|(^card-?expiry-?year$)"
     "|(^card-?expiry-?yr$)"
     "|(^an-?validite$)"
     "|(^an-?expiration$)"
     "|(^annee-?validite$)"
     "|(^annee-?expiration$)"
     "|(^expiry-?date-?field-?year$)"
     "|(^expiration-?date-?year$)"
     "|(^cb-?date-?ann$)"
     "|(^expiration-?date-?yy$)"
     "|(^expiration-?date-?yyyy$)"
     "|(^validity-?year$)"
     "|(^exp-?date-?year$)"
     "|(^date-?y$)"},

    {RegexKey::CC_TYPE,
     "(^cc-?type$)"
     "|(^card-?type$)"
     "|(^card-?brand$)"
     "|(^cc-?brand$)"
     "|(^cb-?type$)"},
};

//=========================================================================
// These rules are from Chromium source codes [1]. Most of them
// converted to JS format have the same meaning with the original ones except
// the first line of "address-level1".
// [1]
// https://source.chromium.org/chromium/chromium/src/+/master:components/autofill/core/common/autofill_regex_constants.cc
const Rule kChromiumRules[] = {
    // ==== Name Fields ====
    {RegexKey::CC_NAME,
     "card.?(?:holder|owner)|name.*(\\b)?on(\\b)?.*card"
     "|(?:card|cc).?name|cc.?full.?name"
     "|(?:card|cc).?owner"
     "|nombre.*tarjeta"                 // es
     "|nom.*carte"                      // fr-FR
     "|nome.*cart"                      // it-IT
     "|名前"                            // ja-JP
     "|Имя.*карты"                      // ru
     "|信用卡开户名|开户名|持卡人姓名"  // zh-CN
     "|持卡人姓名"},                    // zh-TW

    // ==== Credit Card Fields ====
    // Note: `cc-name` expression has been moved up, above `name`, in
    // order to handle specialization through ordering.
    {
        RegexKey::CC_NUMBER,
        "(add)?(?:card|cc|acct).?(?:number|#|no|num)"
        // TODO - lookbehind not supported
        // "|(?<!telefon|haus|person|fødsels|zimmer)nummer" // de-DE,
        // sv-SE, no
        "|カード番号"           // ja-JP
        "|Номер.*карты"         // ru
        "|信用卡号|信用卡号码"  // zh-CN
        "|信用卡卡號"           // zh-TW
        "|카드"                 // ko-KR
                                // es/pt/fr (TODO: lookahead not supported)
        // "|(numero|número|numéro)(?!.*(document|fono|phone|réservation))"
    },

    {RegexKey::CC_EXP_MONTH,
     "exp.*mo|ccmonth|cardmonth|addmonth"
     "|monat"  // de-DE
               // "|fecha" // es
               // "|date.*exp" // fr-FR
               // "|scadenza" // it-IT
               // "|有効期限" // ja-JP
               // "|validade" // pt-BR, pt-PT
               // "|Срок действия карты" // ru
     "|月"},   // zh-CN

    {RegexKey::CC_EXP_YEAR,
     "(add)?year"
     "|jahr"         // de-DE
                     // "|fecha" // es
                     // "|scadenza" // it-IT
                     // "|有効期限" // ja-JP
                     // "|validade" // pt-BR, pt-PT
                     // "|Срок действия карты" // ru
     "|年|有效期"},  // zh-CN

    {RegexKey::CC_EXP,
     "expir|exp.*date|^expfield$"
     "|ablaufdatum|gueltig|gültig"  // de-DE
     "|fecha"                       // es
     "|date.*exp"                   // fr-FR
     "|scadenza"                    // it-IT
     "|有効期限"                    // ja-JP
     "|validade"                    // pt-BR, pt-PT
     "|Срок действия карты"},       // ru
};

static double Sigmoid(double x) { return 1.0 / (1.0 + exp(-x)); }

class FormAutofillImpl {
 public:
  FormAutofillImpl();

  void GetFormAutofillConfidences(
      GlobalObject& aGlobal, const Sequence<OwningNonNull<Element>>& aElements,
      nsTArray<FormAutofillConfidences>& aResults, ErrorResult& aRv);

 private:
  const regex::ffi::RegexWrapper& GetRegex(RegexKey key);

  bool StringMatchesRegExp(const nsACString& str, RegexKey key);
  bool StringMatchesRegExp(const nsAString& str, RegexKey key);
  size_t CountRegExpMatches(const nsACString& str, RegexKey key);
  size_t CountRegExpMatches(const nsAString& str, RegexKey key);
  bool IdOrNameMatchRegExp(Element& element, RegexKey key);
  bool NextFieldMatchesExpYearAutocomplete(Element* aNextField);
  bool PreviousFieldMatchesExpMonthAutocomplete(Element* aPrevField);
  bool LabelMatchesRegExp(const nsTArray<nsCString>* labels, RegexKey key);
  bool PlaceholderMatchesRegExp(Element& element, RegexKey key);
  bool AriaLabelMatchesRegExp(Element& element, RegexKey key);
  bool AutocompleteStringMatches(Element& aElement, const nsAString& aKey);

  bool HasTemplatedValue(Element& element);
  bool MaxLengthIs(Element& aElement, int32_t aValue);
  bool IsExpirationMonthLikely(Element& element);
  bool IsExpirationYearLikely(Element& element);
  bool IsVisible(Element& element);
  bool InputTypeNotNumbery(Element& element);
  bool IsSelectWithCreditCardOptions(Element& element);
  bool IsRadioWithCreditCardText(Element& element,
                                 const nsTArray<nsCString>* labels,
                                 ErrorResult& aRv);
  bool MatchesExpYearAutocomplete(Element& element);
  bool RoleIsMenu(Element& element);

  Element* FindRootForField(Element* aElement);

  Element* FindField(const Sequence<OwningNonNull<Element>>& aElements,
                     uint32_t aStartIndex, int8_t aDirection);
  Element* NextField(const Sequence<OwningNonNull<Element>>& aElements,
                     uint32_t aStartIndex);
  Element* PrevField(const Sequence<OwningNonNull<Element>>& aElements,
                     uint32_t aStartIndex);

  // Array contains regular expressions to match the corresponding
  // field. Ex, CC number, CC type, etc.
  using RegexStringArray =
      EnumeratedArray<RegexKey, RegexKey::Count, nsCString>;
  RegexStringArray mRuleMap;

  // Array that holds RegexWrapper that created by regex::ffi::regex_new
  using RegexWrapperArray =
      EnumeratedArray<RegexKey, RegexKey::Count,
                      UniquePtr<regex::ffi::RegexWrapper>>;
  RegexWrapperArray mRegexes;
};

FormAutofillImpl::FormAutofillImpl() {
  const Rule* rulesets[] = {&kFirefoxRules[0], &kBitwardenRules[0],
                            &kChromiumRules[0]};
  size_t rulesetLengths[] = {ArrayLength(kFirefoxRules),
                             ArrayLength(kBitwardenRules),
                             ArrayLength(kChromiumRules)};

  for (uint32_t i = 0; i < ArrayLength(rulesetLengths); ++i) {
    for (uint32_t j = 0; j < rulesetLengths[i]; ++j) {
      nsCString& rule = mRuleMap[rulesets[i][j].key];
      if (!rule.IsEmpty()) {
        rule.Append("|");
      }
      rule.Append(rulesets[i][j].pattern);
    }
  }
}

const regex::ffi::RegexWrapper& FormAutofillImpl::GetRegex(RegexKey aKey) {
  if (!mRegexes[aKey]) {
    mRegexes[aKey].reset(regex::ffi::regex_new(&mRuleMap[aKey]));
  }
  return *mRegexes[aKey];
}

bool FormAutofillImpl::StringMatchesRegExp(const nsACString& aStr,
                                           RegexKey aKey) {
  return regex::ffi::regex_is_match(&GetRegex(aKey), &aStr);
}

bool FormAutofillImpl::StringMatchesRegExp(const nsAString& aStr,
                                           RegexKey aKey) {
  return StringMatchesRegExp(NS_ConvertUTF16toUTF8(aStr), aKey);
}

size_t FormAutofillImpl::CountRegExpMatches(const nsACString& aStr,
                                            RegexKey aKey) {
  return regex::ffi::regex_count_matches(&GetRegex(aKey), &aStr);
}

size_t FormAutofillImpl::CountRegExpMatches(const nsAString& aStr,
                                            RegexKey aKey) {
  return CountRegExpMatches(NS_ConvertUTF16toUTF8(aStr), aKey);
}

bool FormAutofillImpl::NextFieldMatchesExpYearAutocomplete(
    Element* aNextField) {
  return AutocompleteStringMatches(*aNextField, u"cc-exp-year"_ns);
}

bool FormAutofillImpl::PreviousFieldMatchesExpMonthAutocomplete(
    Element* aPrevField) {
  return AutocompleteStringMatches(*aPrevField, u"cc-exp-month"_ns);
}

bool FormAutofillImpl::IdOrNameMatchRegExp(Element& aElement, RegexKey key) {
  nsAutoString str;
  aElement.GetId(str);
  if (StringMatchesRegExp(str, key)) {
    return true;
  }
  aElement.GetAttr(nsGkAtoms::name, str);
  return StringMatchesRegExp(str, key);
}

bool FormAutofillImpl::LabelMatchesRegExp(
    const nsTArray<nsCString>* labelStrings, RegexKey key) {
  if (!labelStrings) {
    return false;
  }

  for (const auto& str : *labelStrings) {
    if (StringMatchesRegExp(str, key)) {
      return true;
    }
  }
  return false;
}

bool FormAutofillImpl::PlaceholderMatchesRegExp(Element& aElement,
                                                RegexKey aKey) {
  nsAutoString str;
  if (!aElement.GetAttr(nsGkAtoms::placeholder, str)) {
    return false;
  }
  return StringMatchesRegExp(str, aKey);
}

bool FormAutofillImpl::AriaLabelMatchesRegExp(Element& aElement,
                                              RegexKey aKey) {
  nsAutoString str;
  if (!aElement.GetAttr(nsGkAtoms::aria_label, str)) {
    return false;
  }
  return StringMatchesRegExp(str, aKey);
}

bool FormAutofillImpl::AutocompleteStringMatches(Element& aElement,
                                                 const nsAString& aKey) {
  Nullable<AutocompleteInfo> info;
  if (auto* input = HTMLInputElement::FromNode(aElement)) {
    input->GetAutocompleteInfo(info);
  } else {
    AutocompleteInfo autoInfo;
    if (auto* select = HTMLSelectElement::FromNode(aElement)) {
      select->GetAutocompleteInfo(autoInfo);
      info.SetValue(autoInfo);
    }
  }

  if (info.IsNull()) {
    return false;
  }

  return info.Value().mFieldName.Equals(aKey);
}

bool FormAutofillImpl::HasTemplatedValue(Element& aElement) {
  nsAutoString str;
  if (!aElement.GetAttr(nsGkAtoms::value, str)) {
    return false;
  }
  return StringMatchesRegExp(str, RegexKey::TEMPLATED_VALUE);
}

bool FormAutofillImpl::RoleIsMenu(Element& aElement) {
  return aElement.AttrValueIs(kNameSpaceID_None, nsGkAtoms::role,
                              nsGkAtoms::menu, eCaseMatters);
}

/**
 * This function transforms the js implementation of isVisible in fathom.js
 * to C++
 */
bool FormAutofillImpl::IsVisible(Element& aElement) {
  nsIFrame* frame = aElement.GetPrimaryFrame();
  if (!frame) {
    return false;
  }

  // Alternative to reading ``display: none`` due to Bug 1381071.
  // We don't check whether the element is zero-sized and overflow !== 'hidden'
  // like we do in fathom.jsm because if the element is display: none then the
  // frame is null, which we've already checked in the beginning of this
  // function.

  // This corresponding to `if (elementStyle.visibility === 'hidden')` check
  // in isVisible (fathom.jsm)
  const ComputedStyle& style = *frame->Style();
  if (!style.StyleVisibility()->IsVisible()) {
    return false;
  }

  // Check if the element is irrevocably off-screen:
  nsRect elementRect = frame->GetBoundingClientRect();
  if (elementRect.XMost() < 0 || elementRect.YMost() < 0) {
    return false;
  }

  // Below is the translation of how we travese the ancestor chain in
  // `isVisible`
  if (style.IsInOpacityZeroSubtree()) {
    return false;
  }

  // Zero-sized ancestors don’t make descendants hidden unless the
  // descendant has ``overflow: hidden``
  // FIXME: That's wrong, this should check whether the ancestor
  // https://github.com/mozilla/fathom/issues/300
  bool elementIsHidden =
      style.StyleDisplay()->mOverflowX == StyleOverflow::Hidden &&
      style.StyleDisplay()->mOverflowY == StyleOverflow::Hidden;
  if (elementIsHidden) {
    for (; frame; frame = frame->GetInFlowParent()) {
      if (frame->GetRect().IsEmpty()) {
        return false;
      }
    }
  }
  return true;
}

bool FormAutofillImpl::InputTypeNotNumbery(Element& aElement) {
  auto* input = HTMLInputElement::FromNode(aElement);
  if (!input) {
    return true;
  }

  auto type = input->ControlType();
  return type != FormControlType::InputText &&
         type != FormControlType::InputTel &&
         type != FormControlType::InputNumber;
}

bool FormAutofillImpl::IsSelectWithCreditCardOptions(Element& aElement) {
  auto* select = HTMLSelectElement::FromNode(aElement);
  if (!select) {
    return false;
  }

  nsCOMPtr<nsIHTMLCollection> options = select->Options();
  for (uint32_t i = 0; i < options->Length(); ++i) {
    auto* item = options->Item(i);
    auto* option = HTMLOptionElement::FromNode(item);
    if (!option) {
      continue;
    }
    // Bug 1756799, consider using getAttribute("value") instead of .value
    nsAutoString str;
    option->GetValue(str);
    if (StringMatchesRegExp(str, RegexKey::CREDIT_CARD_NETWORK_EXACT_MATCH) ||
        StringMatchesRegExp(str, RegexKey::CREDIT_CARD_NETWORK_LONG)) {
      return true;
    }

    option->GetText(str);
    if (StringMatchesRegExp(str, RegexKey::CREDIT_CARD_NETWORK_EXACT_MATCH) ||
        StringMatchesRegExp(str, RegexKey::CREDIT_CARD_NETWORK_LONG)) {
      return true;
    }
  }
  return false;
}

bool FormAutofillImpl::IsRadioWithCreditCardText(
    Element& aElement, const nsTArray<nsCString>* aLabels, ErrorResult& aRv) {
  auto* input = HTMLInputElement::FromNode(aElement);
  if (!input) {
    return false;
  }
  auto type = input->ControlType();
  if (type != FormControlType::InputRadio) {
    return false;
  }

  nsAutoString str;
  input->GetValue(str, CallerType::System);
  if (CountRegExpMatches(str, RegexKey::CREDIT_CARD_NETWORK) == 1) {
    return true;
  }

  if (aLabels) {
    size_t labelsMatched = 0;
    for (const auto& label : *aLabels) {
      size_t labelMatches =
          CountRegExpMatches(label, RegexKey::CREDIT_CARD_NETWORK);
      if (labelMatches > 1) {
        return false;
      }
      if (labelMatches > 0) {
        labelsMatched++;
      }
    }

    if (labelsMatched) {
      return labelsMatched == 1;
    }
  }

  // Bug 1756798 : Remove reading text content in a <input>
  nsAutoString text;
  aElement.GetTextContent(text, aRv);
  if (aRv.Failed()) {
    return false;
  }
  return CountRegExpMatches(text, RegexKey::CREDIT_CARD_NETWORK) == 1;
}

bool FormAutofillImpl::MaxLengthIs(Element& aElement, int32_t aValue) {
  auto* input = HTMLInputElement::FromNode(aElement);
  if (!input) {
    return false;
  }
  return input->MaxLength() == aValue;
}

static bool TestOptionElementForInteger(Element* aElement, int32_t aTestValue) {
  auto* option = HTMLOptionElement::FromNodeOrNull(aElement);
  if (!option) {
    return false;
  }
  nsAutoString str;
  option->GetValue(str);
  nsContentUtils::ParseHTMLIntegerResultFlags parseFlags;
  int32_t val = nsContentUtils::ParseHTMLInteger(str, &parseFlags);
  if (val == aTestValue) {
    return true;
  }
  option->GetRenderedLabel(str);
  val = nsContentUtils::ParseHTMLInteger(str, &parseFlags);
  return val == aTestValue;
}

static bool MatchOptionContiguousInteger(HTMLOptionsCollection* aOptions,
                                         uint32_t aNumContiguous,
                                         int32_t aInteger) {
  uint32_t len = aOptions->Length();
  if (aNumContiguous > len) {
    return false;
  }

  for (uint32_t i = 0; i <= aOptions->Length() - aNumContiguous; i++) {
    bool match = true;
    for (uint32_t j = 0; j < aNumContiguous; j++) {
      if (!TestOptionElementForInteger(aOptions->GetElementAt(i + j),
                                       aInteger + j)) {
        match = false;
        break;
      }
    }
    if (match) {
      return true;
    }
  }
  return false;
}

bool FormAutofillImpl::IsExpirationYearLikely(Element& aElement) {
  auto* select = HTMLSelectElement::FromNode(aElement);
  if (!select) {
    return false;
  }

  auto* options = select->Options();
  if (!options) {
    return false;
  }

  PRExplodedTime tm;
  PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &tm);
  uint16_t currentYear = tm.tm_year;

  return MatchOptionContiguousInteger(options, 3, currentYear);
}

bool FormAutofillImpl::IsExpirationMonthLikely(Element& aElement) {
  auto* select = HTMLSelectElement::FromNode(aElement);
  if (!select) {
    return false;
  }

  auto* options = select->Options();
  if (!options) {
    return false;
  }

  if (options->Length() != 12 && options->Length() != 13) {
    return false;
  }

  return MatchOptionContiguousInteger(options, 12, 1);
}

Element* FormAutofillImpl::FindRootForField(Element* aElement) {
  if (const auto* control =
          nsGenericHTMLFormControlElement::FromNode(aElement)) {
    if (Element* form = control->GetForm()) {
      return form;
    }
  }

  return aElement->OwnerDoc()->GetDocumentElement();
}

Element* FormAutofillImpl::FindField(
    const Sequence<OwningNonNull<Element>>& aElements, uint32_t aStartIndex,
    int8_t aDirection) {
  MOZ_ASSERT(aDirection == 1 || aDirection == -1);
  MOZ_ASSERT(aStartIndex < aElements.Length());

  Element* curFieldRoot = FindRootForField(aElements[aStartIndex]);
  bool isRootForm = curFieldRoot->IsHTMLElement(nsGkAtoms::form);

  uint32_t num =
      aDirection == 1 ? aElements.Length() - aStartIndex - 1 : aStartIndex;
  for (uint32_t i = 0, searchIndex = aStartIndex; i < num; i++) {
    searchIndex += aDirection;
    const auto& element = aElements[searchIndex];
    Element* root = FindRootForField(element);

    if (isRootForm) {
      // Only search fields that are within the same root element.
      if (curFieldRoot != root) {
        return nullptr;
      }
    } else {
      // Exclude elements inside the rootElement that are already in a <form>.
      if (root->IsHTMLElement(nsGkAtoms::form)) {
        continue;
      }
    }

    if (element->IsAnyOfHTMLElements(nsGkAtoms::input, nsGkAtoms::select)) {
      return element.get();
    }
  }

  return nullptr;
}

Element* FormAutofillImpl::NextField(
    const Sequence<OwningNonNull<Element>>& aElements, uint32_t aStartIndex) {
  return FindField(aElements, aStartIndex, 1);
}

Element* FormAutofillImpl::PrevField(
    const Sequence<OwningNonNull<Element>>& aElements, uint32_t aStartIndex) {
  return FindField(aElements, aStartIndex, -1);
}

static void ExtractLabelStrings(nsINode* aNode, nsTArray<nsCString>& aStrings,
                                ErrorResult& aRv) {
  if (aNode->IsAnyOfHTMLElements(nsGkAtoms::script, nsGkAtoms::noscript,
                                 nsGkAtoms::option, nsGkAtoms::style)) {
    return;
  }

  if (aNode->IsText() || !aNode->HasChildren()) {
    nsAutoString text;
    aNode->GetTextContent(text, aRv);
    if (aRv.Failed()) {
      return;
    }

    text.Trim(kWhitespace);
    CopyUTF16toUTF8(text, *aStrings.AppendElement());
    return;
  }

  for (nsINode* child = aNode->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (child->IsElement() || child->IsText()) {
      ExtractLabelStrings(child, aStrings, aRv);
      if (aRv.Failed()) {
        return;
      }
    }
  }
}

nsTArray<nsCString>* GetLabelStrings(
    Element* aElement,
    const nsTHashMap<void*, nsTArray<nsCString>>& aElementMap,
    const nsTHashMap<nsAtom*, nsTArray<nsCString>>& aIdMap) {
  if (!aElement) {
    return nullptr;
  }

  if (nsAtom* idAtom = aElement->GetID()) {
    return aIdMap.Lookup(idAtom).DataPtrOrNull();
  }

  return aElementMap.Lookup(aElement).DataPtrOrNull();
}

void FormAutofillImpl::GetFormAutofillConfidences(
    GlobalObject& aGlobal, const Sequence<OwningNonNull<Element>>& aElements,
    nsTArray<FormAutofillConfidences>& aResults, ErrorResult& aRv) {
  if (aElements.IsEmpty()) {
    return;
  }

  // Create Labels
  auto* document = aElements[0]->OwnerDoc();
#ifdef DEBUG
  for (uint32_t i = 1; i < aElements.Length(); ++i) {
    MOZ_ASSERT(document == aElements[i]->OwnerDoc());
  }
#endif

  RefPtr<nsContentList> labels = document->GetElementsByTagName(u"label"_ns);
  nsTHashMap<void*, nsTArray<nsCString>> elementsToLabelStrings;
  nsTHashMap<nsAtom*, nsTArray<nsCString>> elementsIdToLabelStrings;
  if (labels) {
    for (uint32_t i = 0; i < labels->Length(); ++i) {
      auto* item = labels->Item(i);
      auto* label = HTMLLabelElement::FromNode(item);
      if (NS_WARN_IF(!label)) {
        continue;
      }
      auto* control = label->GetControl();
      if (!control) {
        continue;
      }
      nsTArray<nsCString> labelStrings;
      ExtractLabelStrings(label, labelStrings, aRv);
      if (aRv.Failed()) {
        return;
      }

      // We need two maps here to keep track controls with id and without id.
      // We can't just use map without id to cover all cases  because there
      // might be multiple elements with the same id.
      if (control->GetID()) {
        elementsIdToLabelStrings.LookupOrInsert(control->GetID())
            .AppendElements(std::move(labelStrings));
      } else {
        elementsToLabelStrings.LookupOrInsert(control).AppendElements(
            std::move(labelStrings));
      }
    }
  }

  nsTArray<AutofillParams> paramSet;
  paramSet.SetLength(aElements.Length());

  for (uint32_t i = 0; i < aElements.Length(); ++i) {
    auto& params = paramSet[i];
    const auto& element = aElements[i];

    Element* nextFillableField = NextField(aElements, i);
    Element* prevFillableField = PrevField(aElements, i);

    const nsTArray<nsCString>* labelStrings = GetLabelStrings(
        element, elementsToLabelStrings, elementsIdToLabelStrings);
    const nsTArray<nsCString>* nextLabelStrings = GetLabelStrings(
        nextFillableField, elementsToLabelStrings, elementsIdToLabelStrings);
    const nsTArray<nsCString>* prevLabelStrings = GetLabelStrings(
        prevFillableField, elementsToLabelStrings, elementsIdToLabelStrings);

    bool idOrNameMatchDwfrmAndBml =
        IdOrNameMatchRegExp(element, RegexKey::DWFRM) &&
        IdOrNameMatchRegExp(element, RegexKey::BML);
    bool idOrNameMatchFirstAndLast =
        IdOrNameMatchRegExp(element, RegexKey::FIRST) &&
        IdOrNameMatchRegExp(element, RegexKey::LAST);
    bool hasTemplatedValue = HasTemplatedValue(element);
    bool isNotVisible = !IsVisible(element);
    bool inputTypeNotNumbery = InputTypeNotNumbery(element);
    bool idOrNameMatchSubscription =
        IdOrNameMatchRegExp(element, RegexKey::SUBSCRIPTION);
    bool roleIsMenu = RoleIsMenu(element);

#define RULE_IMPL2(rule, type) params.m##type##Params[type##Params::rule]
#define RULE_IMPL(rule, type) RULE_IMPL2(rule, type)
#define RULE(rule) RULE_IMPL(rule, RULE_TYPE)

    // cc-number
#define RULE_TYPE CCNumber
    RULE(idOrNameMatchNumberRegExp) =
        IdOrNameMatchRegExp(element, RegexKey::CC_NUMBER);
    RULE(labelsMatchNumberRegExp) =
        LabelMatchesRegExp(labelStrings, RegexKey::CC_NUMBER);
    RULE(placeholderMatchesNumberRegExp) =
        PlaceholderMatchesRegExp(element, RegexKey::CC_NUMBER);
    RULE(ariaLabelMatchesNumberRegExp) =
        AriaLabelMatchesRegExp(element, RegexKey::CC_NUMBER);
    RULE(idOrNameMatchGift) = IdOrNameMatchRegExp(element, RegexKey::GIFT);
    RULE(labelsMatchGift) = LabelMatchesRegExp(labelStrings, RegexKey::GIFT);
    RULE(placeholderMatchesGift) =
        PlaceholderMatchesRegExp(element, RegexKey::GIFT);
    RULE(ariaLabelMatchesGift) =
        AriaLabelMatchesRegExp(element, RegexKey::GIFT);
    RULE(idOrNameMatchSubscription) = idOrNameMatchSubscription;
    RULE(idOrNameMatchDwfrmAndBml) = idOrNameMatchDwfrmAndBml;
    RULE(hasTemplatedValue) = hasTemplatedValue;
    RULE(isNotVisible) = isNotVisible;
    RULE(inputTypeNotNumbery) = inputTypeNotNumbery;
#undef RULE_TYPE

    // cc-name
#define RULE_TYPE CCName
    RULE(idOrNameMatchNameRegExp) =
        IdOrNameMatchRegExp(element, RegexKey::CC_NAME);
    RULE(labelsMatchNameRegExp) =
        LabelMatchesRegExp(labelStrings, RegexKey::CC_NAME);
    RULE(placeholderMatchesNameRegExp) =
        PlaceholderMatchesRegExp(element, RegexKey::CC_NAME);
    RULE(ariaLabelMatchesNameRegExp) =
        AriaLabelMatchesRegExp(element, RegexKey::CC_NAME);
    RULE(idOrNameMatchFirst) = IdOrNameMatchRegExp(element, RegexKey::FIRST);
    RULE(labelsMatchFirst) = LabelMatchesRegExp(labelStrings, RegexKey::FIRST);
    RULE(placeholderMatchesFirst) =
        PlaceholderMatchesRegExp(element, RegexKey::FIRST);
    RULE(ariaLabelMatchesFirst) =
        AriaLabelMatchesRegExp(element, RegexKey::FIRST);
    RULE(idOrNameMatchLast) = IdOrNameMatchRegExp(element, RegexKey::LAST);
    RULE(labelsMatchLast) = LabelMatchesRegExp(labelStrings, RegexKey::LAST);
    RULE(placeholderMatchesLast) =
        PlaceholderMatchesRegExp(element, RegexKey::LAST);
    RULE(ariaLabelMatchesLast) =
        AriaLabelMatchesRegExp(element, RegexKey::LAST);
    RULE(idOrNameMatchSubscription) = idOrNameMatchSubscription;
    RULE(idOrNameMatchFirstAndLast) = idOrNameMatchFirstAndLast;
    RULE(idOrNameMatchDwfrmAndBml) = idOrNameMatchDwfrmAndBml;
    RULE(hasTemplatedValue) = hasTemplatedValue;
    RULE(isNotVisible) = isNotVisible;
#undef RULE_TYPE

    // cc-type
#define RULE_TYPE CCType
    RULE(idOrNameMatchTypeRegExp) =
        IdOrNameMatchRegExp(element, RegexKey::CC_TYPE);
    RULE(labelsMatchTypeRegExp) =
        LabelMatchesRegExp(labelStrings, RegexKey::CC_TYPE);
    RULE(idOrNameMatchVisaCheckout) =
        IdOrNameMatchRegExp(element, RegexKey::VISA_CHECKOUT);
    RULE(ariaLabelMatchesVisaCheckout) =
        AriaLabelMatchesRegExp(element, RegexKey::VISA_CHECKOUT);
    RULE(isSelectWithCreditCardOptions) =
        IsSelectWithCreditCardOptions(element);
    RULE(isRadioWithCreditCardText) =
        IsRadioWithCreditCardText(element, labelStrings, aRv);
    if (aRv.Failed()) {
      return;
    }

    RULE(idOrNameMatchSubscription) = idOrNameMatchSubscription;
    RULE(idOrNameMatchDwfrmAndBml) = idOrNameMatchDwfrmAndBml;
    RULE(hasTemplatedValue) = hasTemplatedValue;
#undef RULE_TYPE

    // cc-exp
#define RULE_TYPE CCExp
    RULE(labelsMatchExpRegExp) =
        LabelMatchesRegExp(labelStrings, RegexKey::CC_EXP);
    RULE(placeholderMatchesExpRegExp) =
        PlaceholderMatchesRegExp(element, RegexKey::CC_EXP);
    RULE(labelsMatchExpWith2Or4DigitYear) =
        LabelMatchesRegExp(labelStrings, RegexKey::TWO_OR_FOUR_DIGIT_YEAR);
    RULE(placeholderMatchesExpWith2Or4DigitYear) =
        PlaceholderMatchesRegExp(element, RegexKey::TWO_OR_FOUR_DIGIT_YEAR);
    RULE(labelsMatchMMYY) = LabelMatchesRegExp(labelStrings, RegexKey::MMYY);
    RULE(placeholderMatchesMMYY) =
        PlaceholderMatchesRegExp(element, RegexKey::MMYY);
    RULE(maxLengthIs7) = MaxLengthIs(element, 7);
    RULE(idOrNameMatchSubscription) = idOrNameMatchSubscription;
    RULE(idOrNameMatchDwfrmAndBml) = idOrNameMatchDwfrmAndBml;
    RULE(hasTemplatedValue) = hasTemplatedValue;
    RULE(isExpirationMonthLikely) = IsExpirationMonthLikely(element);
    RULE(isExpirationYearLikely) = IsExpirationYearLikely(element);
    RULE(idOrNameMatchMonth) = IdOrNameMatchRegExp(element, RegexKey::MONTH);
    RULE(idOrNameMatchYear) = IdOrNameMatchRegExp(element, RegexKey::YEAR);
    RULE(idOrNameMatchExpMonthRegExp) =
        IdOrNameMatchRegExp(element, RegexKey::CC_EXP_MONTH);
    RULE(idOrNameMatchExpYearRegExp) =
        IdOrNameMatchRegExp(element, RegexKey::CC_EXP_YEAR);
    RULE(idOrNameMatchValidation) =
        IdOrNameMatchRegExp(element, RegexKey::VALIDATION);
#undef RULE_TYPE

    // cc-exp-month
#define RULE_TYPE CCExpMonth
    RULE(idOrNameMatchExpMonthRegExp) =
        IdOrNameMatchRegExp(element, RegexKey::CC_EXP_MONTH);
    RULE(labelsMatchExpMonthRegExp) =
        LabelMatchesRegExp(labelStrings, RegexKey::CC_EXP_MONTH);
    RULE(placeholderMatchesExpMonthRegExp) =
        PlaceholderMatchesRegExp(element, RegexKey::CC_EXP_MONTH);
    RULE(ariaLabelMatchesExpMonthRegExp) =
        AriaLabelMatchesRegExp(element, RegexKey::CC_EXP_MONTH);
    RULE(idOrNameMatchMonth) = IdOrNameMatchRegExp(element, RegexKey::MONTH);
    RULE(labelsMatchMonth) = LabelMatchesRegExp(labelStrings, RegexKey::MONTH);
    RULE(placeholderMatchesMonth) =
        PlaceholderMatchesRegExp(element, RegexKey::MONTH);
    RULE(ariaLabelMatchesMonth) =
        AriaLabelMatchesRegExp(element, RegexKey::MONTH);
    RULE(nextFieldIdOrNameMatchExpYearRegExp) =
        nextFillableField &&
        IdOrNameMatchRegExp(*nextFillableField, RegexKey::CC_EXP_YEAR);
    RULE(nextFieldLabelsMatchExpYearRegExp) =
        nextFillableField &&
        LabelMatchesRegExp(nextLabelStrings, RegexKey::CC_EXP_YEAR);
    RULE(nextFieldPlaceholderMatchExpYearRegExp) =
        nextFillableField &&
        PlaceholderMatchesRegExp(*nextFillableField, RegexKey::CC_EXP_YEAR);
    RULE(nextFieldAriaLabelMatchExpYearRegExp) =
        nextFillableField &&
        AriaLabelMatchesRegExp(*nextFillableField, RegexKey::CC_EXP_YEAR);
    RULE(nextFieldIdOrNameMatchYear) =
        nextFillableField &&
        IdOrNameMatchRegExp(*nextFillableField, RegexKey::YEAR);
    RULE(nextFieldLabelsMatchYear) =
        nextFillableField &&
        LabelMatchesRegExp(nextLabelStrings, RegexKey::YEAR);
    RULE(nextFieldPlaceholderMatchesYear) =
        nextFillableField &&
        PlaceholderMatchesRegExp(*nextFillableField, RegexKey::YEAR);
    RULE(nextFieldAriaLabelMatchesYear) =
        nextFillableField &&
        AriaLabelMatchesRegExp(*nextFillableField, RegexKey::YEAR);
    RULE(nextFieldMatchesExpYearAutocomplete) =
        nextFillableField &&
        NextFieldMatchesExpYearAutocomplete(nextFillableField);
    RULE(isExpirationMonthLikely) = IsExpirationMonthLikely(element);
    RULE(nextFieldIsExpirationYearLikely) =
        nextFillableField && IsExpirationYearLikely(*nextFillableField);
    RULE(maxLengthIs2) = MaxLengthIs(element, 2);
    RULE(placeholderMatchesMM) =
        PlaceholderMatchesRegExp(element, RegexKey::MM_MONTH);
    RULE(roleIsMenu) = roleIsMenu;
    RULE(idOrNameMatchSubscription) = idOrNameMatchSubscription;
    RULE(idOrNameMatchDwfrmAndBml) = idOrNameMatchDwfrmAndBml;
    RULE(hasTemplatedValue) = hasTemplatedValue;
#undef RULE_TYPE

    // cc-exp-year
#define RULE_TYPE CCExpYear
    RULE(idOrNameMatchExpYearRegExp) =
        IdOrNameMatchRegExp(element, RegexKey::CC_EXP_YEAR);
    RULE(labelsMatchExpYearRegExp) =
        LabelMatchesRegExp(labelStrings, RegexKey::CC_EXP_YEAR);
    RULE(placeholderMatchesExpYearRegExp) =
        PlaceholderMatchesRegExp(element, RegexKey::CC_EXP_YEAR);
    RULE(ariaLabelMatchesExpYearRegExp) =
        AriaLabelMatchesRegExp(element, RegexKey::CC_EXP_YEAR);
    RULE(idOrNameMatchYear) = IdOrNameMatchRegExp(element, RegexKey::YEAR);
    RULE(labelsMatchYear) = LabelMatchesRegExp(labelStrings, RegexKey::YEAR);
    RULE(placeholderMatchesYear) =
        PlaceholderMatchesRegExp(element, RegexKey::YEAR);
    RULE(ariaLabelMatchesYear) =
        AriaLabelMatchesRegExp(element, RegexKey::YEAR);
    RULE(previousFieldIdOrNameMatchExpMonthRegExp) =
        prevFillableField &&
        IdOrNameMatchRegExp(*prevFillableField, RegexKey::CC_EXP_MONTH);
    RULE(previousFieldLabelsMatchExpMonthRegExp) =
        prevFillableField &&
        LabelMatchesRegExp(prevLabelStrings, RegexKey::CC_EXP_MONTH);
    RULE(previousFieldPlaceholderMatchExpMonthRegExp) =
        prevFillableField &&
        PlaceholderMatchesRegExp(*prevFillableField, RegexKey::CC_EXP_MONTH);
    RULE(previousFieldAriaLabelMatchExpMonthRegExp) =
        prevFillableField &&
        AriaLabelMatchesRegExp(*prevFillableField, RegexKey::CC_EXP_MONTH);
    RULE(previousFieldIdOrNameMatchMonth) =
        prevFillableField &&
        IdOrNameMatchRegExp(*prevFillableField, RegexKey::MONTH);
    RULE(previousFieldLabelsMatchMonth) =
        prevFillableField &&
        LabelMatchesRegExp(prevLabelStrings, RegexKey::MONTH);
    RULE(previousFieldPlaceholderMatchesMonth) =
        prevFillableField &&
        PlaceholderMatchesRegExp(*prevFillableField, RegexKey::MONTH);
    RULE(previousFieldAriaLabelMatchesMonth) =
        prevFillableField &&
        AriaLabelMatchesRegExp(*prevFillableField, RegexKey::MONTH);
    RULE(previousFieldMatchesExpMonthAutocomplete) =
        prevFillableField &&
        PreviousFieldMatchesExpMonthAutocomplete(prevFillableField);
    RULE(isExpirationYearLikely) = IsExpirationYearLikely(element);
    RULE(previousFieldIsExpirationMonthLikely) =
        prevFillableField && IsExpirationMonthLikely(*prevFillableField);
    RULE(placeholderMatchesYYOrYYYY) =
        PlaceholderMatchesRegExp(element, RegexKey::YY_OR_YYYY);
    RULE(roleIsMenu) = roleIsMenu;
    RULE(idOrNameMatchSubscription) = idOrNameMatchSubscription;
    RULE(idOrNameMatchDwfrmAndBml) = idOrNameMatchDwfrmAndBml;
    RULE(hasTemplatedValue) = hasTemplatedValue;
#undef RULE_TYPE
#undef RULE_IMPL2
#undef RULE_IMPL
#undef RULE

    // Calculating the final score of each rule
    FormAutofillConfidences score;

#define CALCULATE_SCORE(type, score)                                        \
  for (auto i : MakeEnumeratedRange(type##Params::Count)) {                 \
    (score) += params.m##type##Params[i] * kCoefficents.m##type##Params[i]; \
  }                                                                         \
  (score) = Sigmoid(score + k##type##Bias);

    CALCULATE_SCORE(CCNumber, score.mCcNumber)
    CALCULATE_SCORE(CCName, score.mCcName)
    CALCULATE_SCORE(CCType, score.mCcType)
    CALCULATE_SCORE(CCExp, score.mCcExp)
    CALCULATE_SCORE(CCExpMonth, score.mCcExpMonth)
    CALCULATE_SCORE(CCExpYear, score.mCcExpYear)

#undef CALCULATE_SCORE

    aResults.AppendElement(score);
  }
}

static StaticAutoPtr<FormAutofillImpl> sFormAutofillInstance;

static FormAutofillImpl* GetFormAutofillImpl() {
  if (!sFormAutofillInstance) {
    sFormAutofillInstance = new FormAutofillImpl();
    ClearOnShutdown(&sFormAutofillInstance);
  }
  return sFormAutofillInstance;
}

/* static */
void FormAutofillNative::GetFormAutofillConfidences(
    GlobalObject& aGlobal, const Sequence<OwningNonNull<Element>>& aElements,
    nsTArray<FormAutofillConfidences>& aResults, ErrorResult& aRv) {
  GetFormAutofillImpl()->GetFormAutofillConfidences(aGlobal, aElements,
                                                    aResults, aRv);
}

}  // namespace mozilla::dom
