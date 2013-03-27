/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pref("datareporting.healthreport.currentDaySubmissionFailureCount", 0);
pref("datareporting.healthreport.documentServerURI", "https://data.mozilla.com/");
pref("datareporting.healthreport.documentServerNamespace", "metrics");
pref("datareporting.healthreport.infoURL", "http://www.mozilla.org/legal/privacy/firefox.html#health-report");
pref("datareporting.healthreport.logging.consoleEnabled", true);
pref("datareporting.healthreport.logging.consoleLevel", "Warn");
pref("datareporting.healthreport.lastDataSubmissionFailureTime", "0");
pref("datareporting.healthreport.lastDataSubmissionRequestedTime", "0");
pref("datareporting.healthreport.lastDataSubmissionSuccessfulTime", "0");
pref("datareporting.healthreport.nextDataSubmissionTime", "0");
pref("datareporting.healthreport.pendingDeleteRemoteData", false);

// Health Report is enabled by default on all channels.
pref("datareporting.healthreport.uploadEnabled", true);

pref("datareporting.healthreport.service.enabled", true);
pref("datareporting.healthreport.service.loadDelayMsec", 10000);
pref("datareporting.healthreport.service.loadDelayFirstRunMsec", 60000);

pref("datareporting.healthreport.service.providerCategories",
#if MOZ_UPDATE_CHANNEL == release
    "healthreport-js-provider-default"
#else
    "healthreport-js-provider-default,healthreport-js-provider-@MOZ_UPDATE_CHANNEL@"
#endif
    );

pref("datareporting.healthreport.about.reportUrl",   "https://fhr.cdn.mozilla.net/%LOCALE%/");
