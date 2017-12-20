"use strict";

/**
 * Basic checks to ensure that helpers constructing responses map their
 * destructured arguments properly to the `init` methods. Full testing of the init
 * methods is left to the DOM code.
 */

/* import-globals-from ../../content/PaymentDialog.js */
let dialogGlobal = {};
Services.scriptloader.loadSubScript("chrome://payments/content/paymentDialog.js", dialogGlobal);

/**
 * @param {Object} responseData with properties in the order matching `nsIBasicCardResponseData`
 *                              init method args.
 * @returns {string} serialized card data
 */
function serializeBasicCardResponseData(responseData) {
  return [...Object.entries(responseData)].map(array => array.join(":")).join(";") + ";";
}


add_task(async function test_createBasicCardResponseData_basic() {
  let expected = {
    cardholderName: "John Smith",
    cardNumber: "1234567890",
    expiryMonth: "01",
    expiryYear: "2017",
    cardSecurityCode: "0123",
  };
  let actual = dialogGlobal.PaymentDialog.createBasicCardResponseData(expected);
  let expectedSerialized = serializeBasicCardResponseData(expected);
  do_check_eq(actual.data, expectedSerialized, "Check data");
});

add_task(async function test_createBasicCardResponseData_minimal() {
  let expected = {
    cardNumber: "1234567890",
  };
  let actual = dialogGlobal.PaymentDialog.createBasicCardResponseData(expected);
  let expectedSerialized = serializeBasicCardResponseData(expected);
  do_print(actual.data);
  do_check_eq(actual.data, expectedSerialized, "Check data");
});

add_task(async function test_createBasicCardResponseData_withoutNumber() {
  let data = {
    cardholderName: "John Smith",
    expiryMonth: "01",
    expiryYear: "2017",
    cardSecurityCode: "0123",
  };
  Assert.throws(() => dialogGlobal.PaymentDialog.createBasicCardResponseData(data),
                /NS_ERROR_FAILURE/,
                "Check cardNumber is required");
});

function checkAddress(actual, expected) {
  for (let [propName, propVal] of Object.entries(expected)) {
    if (propName == "addressLines") {
      // Note the singular vs. plural here.
      do_check_eq(actual.addressLine.length, propVal.length, "Check number of address lines");
      for (let [i, line] of expected.addressLines.entries()) {
        do_check_eq(actual.addressLine.queryElementAt(i, Ci.nsISupportsString).data, line,
                    `Check ${propName} line ${i}`);
      }
      continue;
    }
    do_check_eq(actual[propName], propVal, `Check ${propName}`);
  }
}

add_task(async function test_createPaymentAddress_minimal() {
  let data = {
    country: "CA",
  };
  let actual = dialogGlobal.PaymentDialog.createPaymentAddress(data);
  checkAddress(actual, data);
});

add_task(async function test_createPaymentAddress_basic() {
  let data = {
    country: "CA",
    addressLines: [
      "123 Sesame Street",
      "P.O. Box ABC",
    ],
    region: "ON",
    city: "Delhi",
    dependentLocality: "N/A",
    postalCode: "94041",
    sortingCode: "1234",
    languageCode: "en-CA",
    organization: "Mozilla Corporation",
    recipient: "John Smith",
    phone: "+15195555555",
  };
  let actual = dialogGlobal.PaymentDialog.createPaymentAddress(data);
  checkAddress(actual, data);
});

add_task(async function test_createShowResponse_basic() {
  let requestId = "876hmbvfd45hb";
  dialogGlobal.PaymentDialog.request = {
    requestId,
  };

  let cardData = {
    cardholderName: "John Smith",
    cardNumber: "1234567890",
    expiryMonth: "01",
    expiryYear: "2099",
    cardSecurityCode: "0123",
  };
  let methodData = dialogGlobal.PaymentDialog.createBasicCardResponseData(cardData);

  let responseData = {
    acceptStatus: Ci.nsIPaymentActionResponse.PAYMENT_ACCEPTED,
    methodName: "basic-card",
    methodData,
    payerName: "My Name",
    payerEmail: "my.email@example.com",
    payerPhone: "+15195555555",
  };
  let actual = dialogGlobal.PaymentDialog.createShowResponse(responseData);
  for (let [propName, propVal] of Object.entries(actual)) {
    if (typeof(propVal) != "string") {
      continue;
    }
    if (propName == "requestId") {
      do_check_eq(propVal, requestId, `Check ${propName}`);
      continue;
    }
    if (propName == "data") {
      do_check_eq(propVal, serializeBasicCardResponseData(cardData), `Check ${propName}`);
      continue;
    }

    do_check_eq(propVal, responseData[propName], `Check ${propName}`);
  }
});
