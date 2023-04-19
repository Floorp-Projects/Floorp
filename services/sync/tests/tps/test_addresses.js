/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* global Services */
Services.prefs.setBoolPref("services.sync.engine.addresses", true);

EnableEngines(["addresses"]);

var phases = {
  phase1: "profile1",
  phase2: "profile2",
  phase3: "profile1",
};

const address1 = [
  {
    "given-name": "Timothy",
    "additional-name": "John",
    "family-name": "Berners-Lee",
    organization: "World Wide Web Consortium",
    "street-address": "32 Vassar Street\nMIT Room 32-G524",
    "address-level2": "Cambridge",
    "address-level1": "MA",
    "postal-code": "02139",
    country: "US",
    tel: "+16172535702",
    email: "timbl@w3.org",
    changes: {
      organization: "W3C",
    },
    "unknown-1": "an unknown field from another client",
  },
];

const address1_after = [
  {
    "given-name": "Timothy",
    "additional-name": "John",
    "family-name": "Berners-Lee",
    organization: "W3C",
    "street-address": "32 Vassar Street\nMIT Room 32-G524",
    "address-level2": "Cambridge",
    "address-level1": "MA",
    "postal-code": "02139",
    country: "US",
    tel: "+16172535702",
    email: "timbl@w3.org",
    "unknown-1": "an unknown field from another client",
  },
];

const address2 = [
  {
    "given-name": "John",
    "additional-name": "R.",
    "family-name": "Smith",
    organization: "Mozilla",
    "street-address":
      "Geb\u00E4ude 3, 4. Obergeschoss\nSchlesische Stra\u00DFe 27",
    "address-level2": "Berlin",
    "address-level1": "BE",
    "postal-code": "10997",
    country: "DE",
    tel: "+4930983333000",
    email: "timbl@w3.org",
  },
];

Phase("phase1", [[Addresses.add, address1], [Sync]]);

Phase("phase2", [
  [Sync],
  [Addresses.verify, address1],
  [Addresses.modify, address1],
  [Addresses.add, address2],
  [Sync],
]);

Phase("phase3", [
  [Sync],
  [Addresses.verify, address1_after],
  [Addresses.verify, address2],
  [Sync],
]);
