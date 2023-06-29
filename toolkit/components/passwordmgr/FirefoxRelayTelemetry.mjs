/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export const FirefoxRelayTelemetry = {
  recordRelayIntegrationTelemetryEvent(
    eventObject,
    eventMethod,
    eventFlowId,
    eventExtras
  ) {
    Services.telemetry.recordEvent(
      "relay_integration",
      eventMethod,
      eventObject,
      eventFlowId ?? "",
      eventExtras ?? {}
    );
  },

  recordRelayPrefEvent(eventMethod, eventFlowId, eventExtras) {
    this.recordRelayIntegrationTelemetryEvent(
      "pref_change",
      eventMethod,
      eventFlowId,
      eventExtras
    );
  },

  recordRelayOfferedEvent(eventMethod, eventFlowId, scenarioName) {
    return this.recordRelayIntegrationTelemetryEvent(
      "offer_relay",
      eventMethod,
      eventFlowId,
      {
        scenario: scenarioName,
      }
    );
  },

  recordRelayUsernameFilledEvent(eventMethod, eventFlowId, errorCode = 0) {
    return this.recordRelayIntegrationTelemetryEvent(
      "fill_username",
      eventMethod,
      eventFlowId,
      {
        error_code: errorCode + "",
      }
    );
  },

  recordRelayReusePanelEvent(eventMethod, eventFlowId, errorCode = 0) {
    return this.recordRelayIntegrationTelemetryEvent(
      "reuse_panel",
      eventMethod,
      eventFlowId,
      {
        error_code: errorCode + "",
      }
    );
  },

  recordRelayOptInPanelEvent(eventMethod, eventFlowId, eventExtras) {
    return this.recordRelayIntegrationTelemetryEvent(
      "opt_in_panel",
      eventMethod,
      eventFlowId,
      eventExtras
    );
  },
};

export default FirefoxRelayTelemetry;
