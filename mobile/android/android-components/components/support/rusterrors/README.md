Handles errors that come from Rust functions.

This component defines and installs an application-services `ApplicationErrorReporter` class that:
  - Forwords error reports and breadcrumbs to `SentryServices`
  - Reports error counts to Glean
