/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource://gre/modules/CreditCard.jsm");

add_task(function isValidNumber() {
  function testValid(number, shouldPass) {
    if (shouldPass) {
      ok(CreditCard.isValidNumber(number), `${number} should be considered valid`);
    } else {
      ok(!CreditCard.isValidNumber(number), `${number} should not be considered valid`);
    }
  }

  testValid("0000000000000000", true);
  testValid("4929001587121045", true);
  testValid("5103059495477870", true);
  testValid("6011029476355493", true);
  testValid("3589993783099582", true);
  testValid("5415425865751454", true);
  if (CreditCard.isValidNumber("30190729470495")) {
    ok(false, "todo: 14-digit numbers (Diners Club) aren't supported by isValidNumber yet");
  }
  if (CreditCard.isValidNumber("36333851788250")) {
    ok(false, "todo: 14-digit numbers (Diners Club) aren't supported by isValidNumber yet");
  }
  if (CreditCard.isValidNumber("3532596776688495393")) {
    ok(false, "todo: 19-digit numbers (JCB, Discover, Maestro) could have 16-19 digits");
  }
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
  testValid("001064088", true);
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
  function testFormat(number) {
    let format = CreditCard.formatMaskedNumber(number);
    Assert.equal(format.affix, "****", "Affix should always be four asterisks");
    Assert.equal(format.label, number.substr(-4),
       "The label should always be the last four digits of the card number");
  }
  testFormat("************0000");
  testFormat("************1045");
  testFormat("***********6806");
  testFormat("**********0495");
  testFormat("**********8250");
});

add_task(function test_maskNumber() {
  function testMask(number, expected) {
    let card = new CreditCard({number});
    Assert.equal(card.maskedNumber, expected,
       "Masked number should only show the last four digits");
  }
  testMask("0000000000000000", "**** 0000");
  testMask("4929001587121045", "**** 1045");
  testMask("5103059495477870", "**** 7870");
  testMask("6011029476355493", "**** 5493");
  testMask("3589993783099582", "**** 9582");
  testMask("5415425865751454", "**** 1454");
  testMask("344060747836806", "**** 6806");
  Assert.throws(() => (new CreditCard({number: "1234"})).maskedNumber,
    /Invalid credit card number/,
    "Four or less numbers should throw when retrieving the maskedNumber");
});

add_task(function test_longMaskedNumber() {
  function testMask(number, expected) {
    let card = new CreditCard({number});
    Assert.equal(card.longMaskedNumber, expected,
       "Long masked number should show asterisks for all digits but last four");
  }
  testMask("0000000000000000", "************0000");
  testMask("4929001587121045", "************1045");
  testMask("5103059495477870", "************7870");
  testMask("6011029476355493", "************5493");
  testMask("3589993783099582", "************9582");
  testMask("5415425865751454", "************1454");
  testMask("344060747836806", "***********6806");
  Assert.throws(() => (new CreditCard({number: "1234"})).longMaskedNumber,
    /Invalid credit card number/,
    "Four or less numbers should throw when retrieving the maskedNumber");
});

add_task(function test_isValid() {
  function testValid(number, expirationMonth, expirationYear, shouldPass, message) {
    let card = new CreditCard({
      number,
      expirationMonth,
      expirationYear,
    });
    if (shouldPass) {
      ok(card.isValid(), message);
    } else {
      ok(!card.isValid(), message);
    }
  }
  let year = (new Date()).getFullYear();
  let month = (new Date()).getMonth() + 1;

  testValid("0000000000000000", month, year + 2, true,
    "Valid number and future expiry date (two years) should pass");
  testValid("0000000000000000", month + 2, year, true,
    "Valid number and future expiry date (two months) should pass");
  testValid("0000000000000000", month, year, true,
    "Valid number and expiry date equal to this month should pass");
  testValid("0000000000000000", month - 1, year, false,
    "Valid number but overdue expiry date should fail");
  testValid("0000000000000000", month, year - 1, false,
    "Valid number but overdue expiry date (by a year) should fail");
  testValid("0000000000000001", month, year + 2, false,
    "Invalid number but future expiry date should fail");
});

add_task(function test_normalize() {
  Assert.equal((new CreditCard({number: "0000 0000 0000 0000"})).number, "0000000000000000",
    "Spaces should be removed from card number after it is normalized");
  Assert.equal((new CreditCard({number: "0000   0000\t 0000\t0000"})).number, "0000000000000000",
    "Spaces should be removed from card number after it is normalized");
  Assert.equal((new CreditCard({number: "0000-0000-0000-0000"})).number, "0000000000000000",
    "Hyphens should be removed from card number after it is normalized");
  Assert.equal((new CreditCard({number: "0000-0000 0000-0000"})).number, "0000000000000000",
    "Spaces and hyphens should be removed from card number after it is normalized");
  Assert.equal((new CreditCard({number: "0000000000000000"})).number, "0000000000000000",
    "Normalized numbers should not get changed");
  Assert.equal((new CreditCard({number: "0000"})).number, null,
    "Card numbers that are too short get set to null");

  let card = new CreditCard({number: "0000000000000000"});
  card.expirationYear = "22";
  card.expirationMonth = "11";
  Assert.equal(card.expirationYear, 2022, "Years less than four digits are in the third millenium");
  card.expirationYear = "-200";
  ok(isNaN(card.expirationYear), "Negative years are blocked");
  card.expirationYear = "1998";
  Assert.equal(card.expirationYear, 1998, "Years with four digits are not changed");
  card.expirationYear = "test";
  ok(isNaN(card.expirationYear), "non-number years are returned as NaN");
  card.expirationMonth = "02";
  Assert.equal(card.expirationMonth, 2, "Zero-leading months are converted properly (not octal)");
  card.expirationMonth = "test";
  ok(isNaN(card.expirationMonth), "non-number months are returned as NaN");
  card.expirationMonth = "12";
  Assert.equal(card.expirationMonth, 12, "Months formatted correctly are unchanged");
  card.expirationMonth = "13";
  ok(isNaN(card.expirationMonth), "Months above 12 are blocked");
  card.expirationMonth = "7";
  Assert.equal(card.expirationMonth, 7, "Changing back to a valid number passes");
  card.expirationMonth = "0";
  ok(isNaN(card.expirationMonth), "Months below 1 are blocked");

  card.expirationMonth = card.expirationYear = undefined;
  card.expirationString = "2022/01";
  Assert.equal(card.expirationMonth, 1, "Month should be parsed correctly");
  Assert.equal(card.expirationYear, 2022, "Year should be parsed correctly");
  card.expirationString = "2023-02";
  Assert.equal(card.expirationMonth, 2, "Month should be parsed correctly");
  Assert.equal(card.expirationYear, 2023, "Year should be parsed correctly");
  card.expirationString = "03-2024";
  Assert.equal(card.expirationMonth, 3, "Month should be parsed correctly");
  Assert.equal(card.expirationYear, 2024, "Year should be parsed correctly");
  card.expirationString = "04/2025";
  Assert.equal(card.expirationMonth, 4, "Month should be parsed correctly");
  Assert.equal(card.expirationYear, 2025, "Year should be parsed correctly");
  card.expirationString = "05/26";
  Assert.equal(card.expirationMonth, 5, "Month should be parsed correctly");
  Assert.equal(card.expirationYear, 2026, "Year should be parsed correctly");
  card.expirationString = "27-6";
  Assert.equal(card.expirationMonth, 6, "Month should be parsed correctly");
  Assert.equal(card.expirationYear, 2027, "Year should be parsed correctly");
  card.expirationString = "07/11";
  Assert.equal(card.expirationMonth, 7, "Ambiguous month should be parsed correctly");
  Assert.equal(card.expirationYear, 2011, "Ambiguous year should be parsed correctly");

  card = new CreditCard({
    number: "0000000000000000",
    expirationMonth: "02",
    expirationYear: "2112",
    expirationString: "06-2066",
  });
  Assert.equal(card.expirationMonth, 2, "expirationString is takes lower precendence than explicit month");
  Assert.equal(card.expirationYear, 2112, "expirationString is takes lower precendence than explicit year");
});

add_task(async function test_label() {
  let testCases = [{
      number: "0000000000000000",
      name: "Rudy Badoody",
      expectedLabel: "0000000000000000, Rudy Badoody",
      expectedMaskedLabel: "**** 0000, Rudy Badoody",
    }, {
      number: "3589993783099582",
      name: "Jimmy Babimmy",
      expectedLabel: "3589993783099582, Jimmy Babimmy",
      expectedMaskedLabel: "**** 9582, Jimmy Babimmy",
    }, {
      number: "************9582",
      name: "Jimmy Babimmy",
      expectedLabel: "**** 9582, Jimmy Babimmy",
      expectedMaskedLabel: "**** 9582, Jimmy Babimmy",
    }, {
      name: "Ricky Bobby",
      expectedLabel: "Ricky Bobby",
      expectedMaskedLabel: "Ricky Bobby",
  }];

  for (let testCase of testCases) {
    let {number, name} = testCase;
    let card = new CreditCard({number, name});

    Assert.equal(await card.getLabel({showNumbers: true}), testCase.expectedLabel,
       "The expectedLabel should be shown when showNumbers is true");
    Assert.equal(await card.getLabel({showNumbers: false}), testCase.expectedMaskedLabel,
       "The expectedMaskedLabel should be shown when showNumbers is false");
  }
});
