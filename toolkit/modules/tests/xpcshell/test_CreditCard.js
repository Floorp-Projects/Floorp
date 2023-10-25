/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { CreditCard } = ChromeUtils.importESModule(
  "resource://gre/modules/CreditCard.sys.mjs"
);

add_task(function isValidNumber() {
  function testValid(number, shouldPass) {
    if (shouldPass) {
      ok(
        CreditCard.isValidNumber(number),
        `${number} should be considered valid`
      );
    } else {
      ok(
        !CreditCard.isValidNumber(number),
        `${number} should not be considered valid`
      );
    }
  }

  testValid("0000000000000000", true);

  testValid("41111111112", false); // passes Luhn but too short
  testValid("4111-1111-112", false); // passes Luhn but too short
  testValid("55555555555544440018", false); // passes Luhn but too long
  testValid("5555 5555 5555 4444 0018", false); // passes Luhn but too long

  testValid("4929001587121045", true);
  testValid("5103059495477870", true);
  testValid("6011029476355493", true);
  testValid("3589993783099582", true);
  testValid("5415425865751454", true);

  testValid("378282246310005", true); // American Express test number
  testValid("371449635398431", true); // American Express test number
  testValid("378734493671000", true); // American Express Corporate test number
  testValid("5610591081018250", true); // Australian BankCard test number
  testValid("6759649826438453", true); // Maestro test number
  testValid("6799990100000000019", true); // 19 digit Maestro test number
  testValid("6799-9901-0000-0000019", true); // 19 digit Maestro test number
  testValid("30569309025904", true); // 14 digit Diners Club test number
  testValid("38520000023237", true); // 14 digit Diners Club test number
  testValid("6011111111111117", true); // Discover test number
  testValid("6011000990139424", true); // Discover test number
  testValid("3530111333300000", true); // JCB test number
  testValid("3566002020360505", true); // JCB test number
  testValid("3532596776688495393", true); // 19-digit JCB number. JCB, Discover, Maestro could have 16-19 digits
  testValid("3532 5967 7668 8495393", true); // 19-digit JCB number. JCB, Discover, Maestro could have 16-19 digits
  testValid("5555555555554444", true); // MasterCard test number
  testValid("5105105105105100", true); // MasterCard test number
  testValid("2221000000000009", true); // 2-series MasterCard test number
  testValid("4111111111111111", true); // Visa test number
  testValid("4012888888881881", true); // Visa test number
  testValid("4222222222222", true); // 13 digit Visa test number
  testValid("4222 2222 22222", true); // 13 digit Visa test number
  testValid("4035 5010 0000 0008", true); // Visadebit/Cartebancaire test number

  testValid("5038146897157463", true);
  testValid("4026313395502338", true);
  testValid("6387060366272981", true);
  testValid("474915027480942", true);
  testValid("924894781317325", true);
  testValid("714816113937185", true);
  testValid("790466087343106", true);
  testValid("474320195408363", true);
  testValid("219211148122351", true);
  testValid("633038472250799", true);
  testValid("354236732906484", true);
  testValid("095347810189325", true);
  testValid("930771457288760", true);
  testValid("3091269135815020", true);
  testValid("5471839082338112", true);
  testValid("0580828863575793", true);
  testValid("5015290610002932", true);
  testValid("9465714503078607", true);
  testValid("4302068493801686", true);
  testValid("2721398408985465", true);
  testValid("6160334316984331", true);
  testValid("8643619970075142", true);
  testValid("0218246069710785", true);
  testValid("0000-0000-0080-4609", true);
  testValid("0000 0000 0222 331", true);
  testValid("344060747836806", true);
  testValid("001064088", false); // too short
  testValid("00-10-64-088", false); // still too short
  testValid("4929001587121046", false);
  testValid("5103059495477876", false);
  testValid("6011029476355494", false);
  testValid("3589993783099581", false);
  testValid("5415425865751455", false);
  testValid("5038146897157462", false);
  testValid("4026313395502336", false);
  testValid("6387060366272980", false);
  testValid("344060747836804", false);
  testValid("30190729470496", false);
  testValid("36333851788255", false);
  testValid("526931005800649", false);
  testValid("724952425140686", false);
  testValid("379761391174135", false);
  testValid("030551436468583", false);
  testValid("947377014076746", false);
  testValid("254848023655752", false);
  testValid("226871580283345", false);
  testValid("708025346034339", false);
  testValid("917585839076788", false);
  testValid("918632588027666", false);
  testValid("9946177098017064", false);
  testValid("4081194386488872", false);
  testValid("3095975979578034", false);
  testValid("3662215692222536", false);
  testValid("6723210018630429", false);
  testValid("4411962856225025", false);
  testValid("8276996369036686", false);
  testValid("4449796938248871", false);
  testValid("3350852696538147", false);
  testValid("5011802870046957", false);
  testValid("0000", false);
});

add_task(function test_formatMaskedNumber() {
  function assertMaskedNumber(input, expected) {
    const actual = CreditCard.formatMaskedNumber(input);
    Assert.equal(actual, expected);
  }
  assertMaskedNumber("************0000", "****0000");
  assertMaskedNumber("************1045", "****1045");
  assertMaskedNumber("***********6806", "****6806");
  assertMaskedNumber("**********0495", "****0495");
  assertMaskedNumber("**********8250", "****8250");
});

add_task(function test_maskNumber() {
  function testMask(number, expected) {
    let card = new CreditCard({ number });
    Assert.equal(
      card.maskedNumber,
      expected,
      "Masked number should only show the last four digits"
    );
  }
  testMask("0000000000000000", "**** 0000");
  testMask("4929001587121045", "**** 1045");
  testMask("5103059495477870", "**** 7870");
  testMask("6011029476355493", "**** 5493");
  testMask("3589993783099582", "**** 9582");
  testMask("5415425865751454", "**** 1454");
  testMask("344060747836806", "**** 6806");
  testMask("6799990100000000019", "**** 0019");
  Assert.throws(
    () => new CreditCard({ number: "1234" }).maskedNumber,
    /Invalid credit card number/,
    "Four or less numbers should throw when retrieving the maskedNumber"
  );
});

add_task(function test_longMaskedNumber() {
  function testMask(number, expected) {
    let card = new CreditCard({ number });
    Assert.equal(
      card.longMaskedNumber,
      expected,
      "Long masked number should show asterisks for all digits but last four"
    );
  }
  testMask("0000000000000000", "************0000");
  testMask("4929001587121045", "************1045");
  testMask("5103059495477870", "************7870");
  testMask("6011029476355493", "************5493");
  testMask("3589993783099582", "************9582");
  testMask("5415425865751454", "************1454");
  testMask("344060747836806", "***********6806");
  testMask("6799990100000000019", "***************0019");
  Assert.throws(
    () => new CreditCard({ number: "1234" }).longMaskedNumber,
    /Invalid credit card number/,
    "Four or less numbers should throw when retrieving the longMaskedNumber"
  );
});

add_task(function test_isValid() {
  function testValid(
    number,
    expirationMonth,
    expirationYear,
    shouldPass,
    message
  ) {
    let isValid = false;
    try {
      let card = new CreditCard({
        number,
        expirationMonth,
        expirationYear,
      });
      isValid = card.isValid();
    } catch (ex) {
      isValid = false;
    }
    if (shouldPass) {
      ok(isValid, message);
    } else {
      ok(!isValid, message);
    }
  }
  let year = new Date().getFullYear();
  let month = new Date().getMonth() + 1;

  testValid(
    "0000000000000000",
    month,
    year + 2,
    true,
    "Valid number and future expiry date (two years) should pass"
  );
  testValid(
    "0000000000000000",
    month < 11 ? month + 2 : month % 10,
    month < 11 ? year : year + 1,
    true,
    "Valid number and future expiry date (two months) should pass"
  );
  testValid(
    "0000000000000000",
    month,
    year,
    true,
    "Valid number and expiry date equal to this month should pass"
  );
  testValid(
    "0000000000000000",
    month > 1 ? month - 1 : 12,
    month > 1 ? year : year - 1,
    false,
    "Valid number but overdue expiry date should fail"
  );
  testValid(
    "0000000000000000",
    month,
    year - 1,
    false,
    "Valid number but overdue expiry date (by a year) should fail"
  );
  testValid(
    "0000000000000001",
    month,
    year + 2,
    false,
    "Invalid number but future expiry date should fail"
  );
});

add_task(function test_normalize() {
  Assert.equal(
    new CreditCard({ number: "0000 0000 0000 0000" }).number,
    "0000000000000000",
    "Spaces should be removed from card number after it is normalized"
  );
  Assert.equal(
    new CreditCard({ number: "0000   0000\t 0000\t0000" }).number,
    "0000000000000000",
    "Spaces should be removed from card number after it is normalized"
  );
  Assert.equal(
    new CreditCard({ number: "0000-0000-0000-0000" }).number,
    "0000000000000000",
    "Hyphens should be removed from card number after it is normalized"
  );
  Assert.equal(
    new CreditCard({ number: "0000-0000 0000-0000" }).number,
    "0000000000000000",
    "Spaces and hyphens should be removed from card number after it is normalized"
  );
  Assert.equal(
    new CreditCard({ number: "0000000000000000" }).number,
    "0000000000000000",
    "Normalized numbers should not get changed"
  );

  let card = new CreditCard({ number: "0000000000000000" });
  card.expirationYear = "22";
  card.expirationMonth = "11";
  Assert.equal(
    card.expirationYear,
    2022,
    "Years less than four digits are in the third millenium"
  );
  card.expirationYear = "-200";
  ok(isNaN(card.expirationYear), "Negative years are blocked");
  card.expirationYear = "1998";
  Assert.equal(
    card.expirationYear,
    1998,
    "Years with four digits are not changed"
  );
  card.expirationYear = "test";
  ok(isNaN(card.expirationYear), "non-number years are returned as NaN");
  card.expirationMonth = "02";
  Assert.equal(
    card.expirationMonth,
    2,
    "Zero-leading months are converted properly (not octal)"
  );
  card.expirationMonth = "test";
  ok(isNaN(card.expirationMonth), "non-number months are returned as NaN");
  card.expirationMonth = "12";
  Assert.equal(
    card.expirationMonth,
    12,
    "Months formatted correctly are unchanged"
  );
  card.expirationMonth = "13";
  ok(isNaN(card.expirationMonth), "Months above 12 are blocked");
  card.expirationMonth = "7";
  Assert.equal(
    card.expirationMonth,
    7,
    "Changing back to a valid number passes"
  );
  card.expirationMonth = "0";
  ok(isNaN(card.expirationMonth), "Months below 1 are blocked");
  card.expirationMonth = card.expirationYear = undefined;
  card.expirationString = "2022/01";
  Assert.equal(card.expirationMonth, 1, "Month should be parsed correctly");
  Assert.equal(card.expirationYear, 2022, "Year should be parsed correctly");

  card.expirationString = "2028 / 05";
  Assert.equal(card.expirationMonth, 5, "Month should be parsed correctly");
  Assert.equal(card.expirationYear, 2028, "Year should be parsed correctly");

  card.expirationString = "2023-02";
  Assert.equal(card.expirationMonth, 2, "Month should be parsed correctly");
  Assert.equal(card.expirationYear, 2023, "Year should be parsed correctly");

  card.expirationString = "2029 - 09";
  Assert.equal(card.expirationMonth, 9, "Month should be parsed correctly");
  Assert.equal(card.expirationYear, 2029, "Year should be parsed correctly");

  card.expirationString = "03-2024";
  Assert.equal(card.expirationMonth, 3, "Month should be parsed correctly");
  Assert.equal(card.expirationYear, 2024, "Year should be parsed correctly");

  card.expirationString = "08 - 2024";
  Assert.equal(card.expirationMonth, 8, "Month should be parsed correctly");
  Assert.equal(card.expirationYear, 2024, "Year should be parsed correctly");

  card.expirationString = "04/2025";
  Assert.equal(card.expirationMonth, 4, "Month should be parsed correctly");
  Assert.equal(card.expirationYear, 2025, "Year should be parsed correctly");

  card.expirationString = "01 / 2023";
  Assert.equal(card.expirationMonth, 1, "Month should be parsed correctly");
  Assert.equal(card.expirationYear, 2023, "Year should be parsed correctly");

  card.expirationString = "05/26";
  Assert.equal(card.expirationMonth, 5, "Month should be parsed correctly");
  Assert.equal(card.expirationYear, 2026, "Year should be parsed correctly");

  card.expirationString = "   06 /  27 ";
  Assert.equal(card.expirationMonth, 6, "Month should be parsed correctly");
  Assert.equal(card.expirationYear, 2027, "Year should be parsed correctly");

  card.expirationString = "04 / 25";
  Assert.equal(card.expirationMonth, 4, "Month should be parsed correctly");
  Assert.equal(card.expirationYear, 2025, "Year should be parsed correctly");

  card.expirationString = "27-6";
  Assert.equal(card.expirationMonth, 6, "Month should be parsed correctly");
  Assert.equal(card.expirationYear, 2027, "Year should be parsed correctly");

  card.expirationString = "26 - 5";
  Assert.equal(card.expirationMonth, 5, "Month should be parsed correctly");
  Assert.equal(card.expirationYear, 2026, "Year should be parsed correctly");

  card.expirationString = "07/11";
  Assert.equal(
    card.expirationMonth,
    7,
    "Ambiguous month should be parsed correctly"
  );
  Assert.equal(
    card.expirationYear,
    2011,
    "Ambiguous year should be parsed correctly"
  );

  card.expirationString = "08 / 12";
  Assert.equal(
    card.expirationMonth,
    8,
    "Ambiguous month should be parsed correctly"
  );
  Assert.equal(
    card.expirationYear,
    2012,
    "Ambiguous year should be parsed correctly"
  );

  card = new CreditCard({
    number: "0000000000000000",
    expirationMonth: "02",
    expirationYear: "2112",
    expirationString: "06-2066",
  });
  Assert.equal(
    card.expirationMonth,
    2,
    "expirationString is takes lower precendence than explicit month"
  );
  Assert.equal(
    card.expirationYear,
    2112,
    "expirationString is takes lower precendence than explicit year"
  );
});

add_task(async function test_label() {
  let testCases = [
    {
      number: "0000000000000000",
      name: "Rudy Badoody",
      expectedMaskedLabel: "**** 0000, Rudy Badoody",
    },
    {
      number: "3589993783099582",
      name: "Jimmy Babimmy",
      expectedMaskedLabel: "**** 9582, Jimmy Babimmy",
    },
  ];

  for (let testCase of testCases) {
    let { number, name } = testCase;
    Assert.equal(
      await CreditCard.getLabel({ number, name }),
      testCase.expectedMaskedLabel,
      "The expectedMaskedLabel should be shown when showNumbers is false"
    );
  }
});

add_task(async function test_network() {
  let supportedNetworks = CreditCard.getSupportedNetworks();
  Assert.ok(
    supportedNetworks.length,
    "There are > 0 supported credit card networks"
  );

  let ccNumber = "0000000000000000";
  let testCases = supportedNetworks.map(network => {
    return { number: ccNumber, network, expectedNetwork: network };
  });
  testCases.push({
    number: ccNumber,
    network: "gringotts",
    expectedNetwork: "gringotts",
  });
  testCases.push({
    number: ccNumber,
    network: "",
    expectedNetwork: undefined,
  });

  for (let testCase of testCases) {
    let { number, network } = testCase;
    let card = new CreditCard({ number, network });
    Assert.equal(
      await card.network,
      testCase.expectedNetwork,
      `The expectedNetwork ${card.network} should match the card network ${testCase.expectedNetwork}`
    );
  }
});

add_task(async function test_isValidNetwork() {
  for (let network of CreditCard.getSupportedNetworks()) {
    Assert.ok(CreditCard.isValidNetwork(network), "supported network is valid");
  }
  Assert.ok(!CreditCard.isValidNetwork(), "undefined is not a valid network");
  Assert.ok(
    !CreditCard.isValidNetwork(""),
    "empty string is not a valid network"
  );
  Assert.ok(!CreditCard.isValidNetwork(null), "null is not a valid network");
  Assert.ok(
    !CreditCard.isValidNetwork("Visa"),
    "network validity is case-sensitive"
  );
  Assert.ok(
    !CreditCard.isValidNetwork("madeupnetwork"),
    "unknown network is invalid"
  );
});

add_task(async function test_getType() {
  const RECOGNIZED_CARDS = [
    // Edge cases
    ["2221000000000000", "mastercard"],
    ["2720000000000000", "mastercard"],
    ["2200000000000000", "mir"],
    ["2204000000000000", "mir"],
    ["340000000000000", "amex"],
    ["370000000000000", "amex"],
    ["3000000000000000", "diners"],
    ["3050000000000000", "diners"],
    ["3095000000000000", "diners"],
    ["36000000000000", "diners"],
    ["3800000000000000", "diners"],
    ["3900000000000000", "diners"],
    ["3528000000000000", "jcb"],
    ["3589000000000000", "jcb"],
    ["4035000000000000", "cartebancaire"],
    ["4360000000000000", "cartebancaire"],
    ["4000000000000000", "visa"],
    ["4999999999999999", "visa"],
    ["5400000000000000", "mastercard"],
    ["5500000000000000", "mastercard"],
    ["5100000000000000", "mastercard"],
    ["5399999999999999", "mastercard"],
    ["6011000000000000", "discover"],
    ["6221260000000000", "discover"],
    ["6229250000000000", "discover"],
    ["6240000000000000", "discover"],
    ["6269990000000000", "discover"],
    ["6282000000000000", "discover"],
    ["6288990000000000", "discover"],
    ["6400000000000000", "discover"],
    ["6500000000000000", "discover"],
    ["6200000000000000", "unionpay"],
    ["8100000000000000", "unionpay"],
    // Valid according to Luhn number
    ["2204941877211882", "mir"],
    ["2720994326581252", "mastercard"],
    ["374542158116607", "amex"],
    ["36006666333344", "diners"],
    ["3541675340715696", "jcb"],
    ["3543769248058305", "jcb"],
    ["4035501428146300", "cartebancaire"],
    ["4111111111111111", "visa"],
    ["5346755600299631", "mastercard"],
    ["5495770093313616", "mastercard"],
    ["5574238524540144", "mastercard"],
    ["6011029459267962", "discover"],
    ["6278592974938779", "unionpay"],
    ["8171999927660000", "unionpay"],
    ["30569309025904", "diners"],
    ["38520000023237", "diners"],
  ];
  for (let [value, type] of RECOGNIZED_CARDS) {
    Assert.equal(
      CreditCard.getType(value),
      type,
      `Expected ${value} to be recognized as ${type}`
    );
  }

  const UNRECOGNIZED_CARDS = [
    ["411111111111111", "15 digits"],
    ["41111111111111111", "17 digits"],
    ["", "empty"],
    ["9111111111111111", "Unknown prefix"],
  ];
  for (let [value, reason] of UNRECOGNIZED_CARDS) {
    Assert.equal(
      CreditCard.getType(value),
      null,
      `Expected ${value} to not match any card because: ${reason}`
    );
  }
});

add_task(async function test_getNetworkFromName() {
  const RECOGNIZED_NAMES = [
    ["amex", "amex"],
    ["American Express", "amex"],
    ["american express", "amex"],
    ["mastercard", "mastercard"],
    ["master card", "mastercard"],
    ["MasterCard", "mastercard"],
    ["Master Card", "mastercard"],
    ["Union Pay", "unionpay"],
    ["UnionPay", "unionpay"],
    ["Unionpay", "unionpay"],
    ["unionpay", "unionpay"],

    ["Unknown", null],
    ["", null],
  ];
  for (let [value, type] of RECOGNIZED_NAMES) {
    Assert.equal(
      CreditCard.getNetworkFromName(value),
      type,
      `Expected ${value} to be recognized as ${type}`
    );
  }
});

add_task(async function test_normalizeCardNumber() {
  let testCases = [
    ["5495770093313616", "5495770093313616"],
    ["5495 7700 9331 3616", "5495770093313616"],
    [" 549 57700 93313 616 ", "5495770093313616"],
    ["5495-7700-9331-3616", "5495770093313616"],
    ["", null],
    [undefined, null],
    [null, null],
  ];
  for (let [input, expected] of testCases) {
    let actual = CreditCard.normalizeCardNumber(input);
    Assert.equal(
      actual,
      expected,
      `Expected ${input} to normalize to ${expected}`
    );
  }
});
