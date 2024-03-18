See https://github.com/mozilla-mobile/fenix/wiki/Telemetry-Checklist for the steps to implement new probes.

# Creating Glean Annotations

Glean Annotations repository: https://github.com/mozilla/glean-annotations

See [Add a Glean Annotation for an event](https://github.com/mozilla-mobile/fenix/wiki/Add-a-Glean-Annotation-for-an-event) for instructions.

More info [here](https://mozilla.github.io/glean-annotations/contributing/creating/)


# Data review

Data reviews are needed on all PRs that add new telemetry or modify existing telemetry.

1. The implementer must complete the forms for [data renewal](https://github.com/mozilla/data-review/blob/main/renewal_request.md) or [a new data request](https://github.com/mozilla/data-review/blob/main/request.md) and put them as a comment in their PR.
2. Once the form is complete, contact a [Data Steward](https://wiki.mozilla.org/Data_Collection) to arrange a review. Note: a data review does not replace code review! The PR should not land without both a data review and a code review.
3. Once the data review is complete, add the link to the approval in the `data_reviews` sub-section of your metric in the `metrics.yaml` file.
Example:

```
download_notification:
  resume:
    type: event
    description: |
      A user resumed a download in the download notification
    bugs:
      - https://github.com/mozilla-mobile/fenix/issues/5583
    data_reviews:
      - https://github.com/mozilla-mobile/fenix/pull/6554
      - https://github.com/mozilla-mobile/fenix/pull/13958#issuecomment-676857877
      - https://github.com/mozilla-mobile/fenix/pull/18143
    data_sensitivity:
      - interaction
    notification_emails:
      - fenix-core@mozilla.com
    expires: "2021-07-01"
```

When a telemetry probe is being renewed, do not remove the old data review links from `metrics.yaml`. The new approval should be added to the existing list.

Make sure you are selecting the correct Category of data that is being collected: https://wiki.mozilla.org/Data_Collection#Data_Collection_Categories

# Renewing existing telemetry

1. Collect a list of metrics from metrics.yaml file in Fenix that will be expiring in that month
  a. Currently compiling these in [this doc](https://docs.google.com/document/d/1NGlnTa9TPyTnd3ciUPbwujbITjkX8p8vJybXcZrrM2w/edit#)
  b. This should be done at least a few weeks prior to the events/metrics' expiration date
  c. Including metric name, original data review PR link, description (if it’s unclear from the name)
2. Figure out the owner for each of the metrics (who needs to give the OK to renew/remove)
  a. Most renewals will need product approval
  b. Other approvals could come from the engineering team (e.g. `preferences.remote_debugging_enabled`), GV, App Services, Performance (e.g. `startup.timeline.framework_primary`), etc.
3. Answer any open questions for the metric owners, and get approval from them to:
  a. Renew the metric (for how long? 6 months? 1 year?)
  b. Choose not to renew (but not delete)
  c. Choose to remove the metric
  d. Renew the metric and set to never expire (this should only be for business critical metrics)
4. Fill out the [renewal request](https://github.com/mozilla/data-review/blob/main/renewal_request.md) for each metric, with question 3 (“Why was the initial period of collection insufficient?”) being answered by the owner
5. Open a PR for the renewals
  a. Sometimes it’s easier to split up the renewals into multiple PRs if there are multiple owners: e.g. [product renewals](https://github.com/mozilla-mobile/fenix/pull/21788), [perf renewals](https://github.com/mozilla-mobile/fenix/pull/21315), [Fission renewals](https://github.com/mozilla-mobile/fenix/pull/21779) for December 2021
6. Get a data review for your [renewal request](https://github.com/mozilla/data-review/blob/main/renewal_request.md) and update each of the metrics’ data-reviews in metrics.yaml to reflect this

## Approval process

For each telemetry probe that we want to renew, the data-review will ask us [these questions](https://github.com/mozilla/data-review/blob/main/renewal_request.md). Each probe/group of related probes should have answers to those questions ([example](https://github.com/mozilla-mobile/fenix/pull/20517#issuecomment-887038794)).

### Example renewal data request
```
# Request for Data Collection Renewal

`search_shortcuts:`
 selected
1) Provide a link to the initial Data Collection Review Request for this collection.
- https://github.com/mozilla-mobile/fenix/pull/1202#issuecomment-476870449
- https://github.com/mozilla-mobile/fenix/pull/15713#issuecomment-703972068
- https://github.com/mozilla-mobile/fenix/pull/19924#issuecomment-861423789

2) When will this collection now expire? 08/01/2022
3) Why was the initial period of collection insufficient?
Important for revenue tracking and optimization.

`Toolbar_settings:`
 changed_position:
1) Provide a link to the initial Data Collection Review Request for this collection.
- https://github.com/mozilla-mobile/fenix/pull/6608
- https://github.com/mozilla-mobile/fenix/pull/15713#issuecomment-703972068
- https://github.com/mozilla-mobile/fenix/pull/19924#issuecomment-861423789

2) When will this collection now expire? 08/01/2022
3) Why was the initial period of collection insufficient?
The data didn’t initially tell us what we wanted (there were bugs), so we want to continue tracking this so we can answer our questions.

`login_dialog:`
 displayed:
cancelled
saved
never_save
1) Provide a link to the initial Data Collection Review Request for this collection.
- https://github.com/mozilla-mobile/fenix/pull/13050
- https://github.com/mozilla-mobile/fenix/pull/19924#issuecomment-861423789

2) When will this collection now expire? 08/01/2022
3) Why was the initial period of collection insufficient?
Still need to optimize this feature and we want trends from 6+mo of data.
```

For product-defined telemetry, this will involve meeting with a product manager and discussing each probe. There are three options: renew the probe for another length of time (usually 6 months), let the probe expire to evaluate later if the probe is still needed, or remove the probe entirely.
