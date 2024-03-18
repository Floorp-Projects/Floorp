## This document outlines how data is collected in Firefox Preview.

### Telemetry

When a user has "Telemetry" enabled under Data Choices in the browser settings, Firefox Preview sends a "core" ping and an "event" ping to Mozilla's telemetry service. "core" ping using the same format documented on firefox-source-docs.mozilla.org.

[Here](https://github.com/mozilla-mobile/fenix/blob/master/docs/metrics.md) is a list of Event Pings, Metrics Pings, and Activation Pings.

**User can disable telemetry by turning the Telemetry toggle off under Data Choices.**


***
### Adjust

See [here](https://github.com/mozilla-mobile/fenix/wiki/Adjust-Usage) for details on Adjust usage in Firefox Preview.

***

### Sentry

Sentry collects a stack trace for each crash in Fenix.

If the user has "Telemetry" enabled under Data Choices in the browser settings, then Sentry collects breadcrumbs containing the name of each Android Fragment in the app. This helps an engineer diagnose the cause of the crash by seeing the internal names of screens visited before the crash occurred, i.e. Browser, Search, Home, etc. No information is stored about any arguments passed to any Fragments.
