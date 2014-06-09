/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["MobileIdentityUIGluePhoneInfo",
                         "MobileIdentityUIGluePromptResult"];

this.MobileIdentityUIGluePhoneInfo = function (aMsisdn, aOperator, aServiceId,
                                               aExternal, aPrimary) {
  this.msisdn = aMsisdn;
  this.operator = aOperator;
  this.serviceId = aServiceId;
  // A phone number is considered "external" when it doesn't or we don't know
  // if it does belong to any of the device SIM cards.
  this.external = aExternal;
  this.primary = aPrimary;
}

this.MobileIdentityUIGluePhoneInfo.prototype = {};

this.MobileIdentityUIGluePromptResult = function (aPhoneNumber, aPrefix, aMcc,
                                                  aServiceId) {
  this.phoneNumber = aPhoneNumber;
  this.prefix = aPrefix;
  this.mcc = aMcc;
  this.serviceId = aServiceId;
}

this.MobileIdentityUIGluePromptResult.prototype = {};
