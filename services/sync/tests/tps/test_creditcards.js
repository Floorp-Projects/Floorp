/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* global Services */
Services.prefs.setBoolPref("services.sync.engine.creditcards", true);

EnableEngines(["creditcards"]);

var phases = {
  "phase1": "profile1",
  "phase2": "profile2",
  "phase3": "profile1"
};

const cc1 = [{
  "cc-name": "John Doe",
  "cc-number": "4716179744040592",
  "cc-exp-month": 4,
  "cc-exp-year": 2050,
  "changes": {
    "cc-exp-year": 2051
  }
}];

const cc1_after = [{
  "cc-name": "John Doe",
  "cc-number": "4716179744040592",
  "cc-exp-month": 4,
  "cc-exp-year": 2051,
}];

const cc2 = [{
  "cc-name": "Timothy Berners-Lee",
  "cc-number": "2221000374457678",
  "cc-exp-month": 12,
  "cc-exp-year": 2050,
}];

Phase("phase1", [
  [CreditCards.add, cc1],
  [Sync]
]);

Phase("phase2", [
  [Sync],
  [CreditCards.verify, cc1],
  [CreditCards.modify, cc1],
  [CreditCards.add, cc2],
  [Sync]
]);

Phase("phase3", [
  [Sync],
  [CreditCards.verifyNot, cc1],
  [CreditCards.verify, cc1_after],
  [CreditCards.verify, cc2]
]);
