/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { _ExperimentManager } = ChromeUtils.import(
  "resource://messaging-system/experiments/ExperimentManager.jsm"
);
const { ExperimentStore } = ChromeUtils.import(
  "resource://messaging-system/experiments/ExperimentStore.jsm"
);
const { NormandyUtils } = ChromeUtils.import(
  "resource://normandy/lib/NormandyUtils.jsm"
);

const EXPORTED_SYMBOLS = ["ExperimentFakes"];

const ExperimentFakes = {
  manager() {
    return new _ExperimentManager({ storeId: "FakeStore" });
  },
  store() {
    return new ExperimentStore("FakeStore");
  },
  experiment(slug, props = {}) {
    return {
      slug,
      active: true,
      enrollmentId: NormandyUtils.generateUuid(),
      branch: { slug: "treatment", value: { title: "hello" } },
      ...props,
    };
  },
  recipe(slug, props = {}) {
    return {
      slug,
      branches: [
        { slug: "control", value: null },
        { slug: "treatment", value: { title: "hello" } },
      ],
      ...props,
    };
  },
};
