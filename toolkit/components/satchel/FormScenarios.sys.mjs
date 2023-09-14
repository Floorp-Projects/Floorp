/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { FormLikeFactory } from "resource://gre/modules/FormLikeFactory.sys.mjs";
import { SignUpFormRuleset } from "resource://gre/modules/SignUpFormRuleset.sys.mjs";
import { FirefoxRelayUtils } from "resource://gre/modules/FirefoxRelayUtils.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

export class FormScenarios {
  /**
   * Caches the scores when running the SignUpFormRuleset against a form
   */
  static #cachedSignUpFormScore = new WeakMap();

  /**
   * Detect usage scenarios of the form.
   *
   * @param {object} options named options
   * @param {HTMLInputElement} [options.input] where current focus is
   * @param {FormLike} [options.form]
   *
   * @returns {Array<string>} detected scenario names
   */
  static detect({ input, form }) {
    const scenarios = {};

    if (!FormScenarios.signupDetectionEnabled) {
      return scenarios;
    }

    // Running simple heuristics first, because running the SignUpFormRuleset is expensive
    if (
      input &&
      // At the moment Relay integration is the only interested party in "sign up form",
      // so we optimize a bit by checking if it's enabled or not.
      FirefoxRelayUtils.isRelayInterestedField(input)
    ) {
      form ??= FormLikeFactory.findRootForField(input);

      scenarios.signUpForm = FormScenarios.#isProbablyASignUpForm(form);
    }

    return scenarios;
  }

  /**
   * Determine if the form is a sign-up form.
   * This is done by running the rules of the Fathom SignUpFormRuleset against the form and calucating a score between 0 and 1.
   * It's considered a sign-up form, if the score is higher than the confidence threshold (default=0.75)
   *
   * @param {HTMLFormElement} formElement
   * @returns {boolean} returns true if the calculcated score is higher than the confidenceThreshold
   */
  static #isProbablyASignUpForm(formElement) {
    let score = FormScenarios.#cachedSignUpFormScore.get(formElement);
    if (!score) {
      TelemetryStopwatch.start("PWMGR_SIGNUP_FORM_DETECTION_MS");
      try {
        const { rules, type } = SignUpFormRuleset;
        const results = rules.against(formElement);
        score = results.get(formElement).scoreFor(type);
        TelemetryStopwatch.finish("PWMGR_SIGNUP_FORM_DETECTION_MS");
      } finally {
        if (TelemetryStopwatch.running("PWMGR_SIGNUP_FORM_DETECTION_MS")) {
          TelemetryStopwatch.cancel("PWMGR_SIGNUP_FORM_DETECTION_MS");
        }
      }
      FormScenarios.#cachedSignUpFormScore.set(formElement, score);
    }

    const threshold = FormScenarios.signupDetectionConfidenceThreshold;
    return score > threshold;
  }
}
XPCOMUtils.defineLazyPreferenceGetter(
  FormScenarios,
  "signupDetectionConfidenceThreshold",
  "signon.signupDetection.confidenceThreshold",
  "0.75"
);
XPCOMUtils.defineLazyPreferenceGetter(
  FormScenarios,
  "signupDetectionEnabled",
  "signon.signupDetection.enabled",
  true
);
