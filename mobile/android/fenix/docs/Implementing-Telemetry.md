## Things to note before implementation:
* Understand that telemetry is important, it is not just a checkmark for feature completion.
* The consumer of the telemetry is the data science team.
* When in doubt, please follow the example implementation, documentation and data review format linked below.
* Avoid using SharedPreferences.
* Write unit tests.

## Procedure to follow before implementing the telemetry:
1. Contact Product team to understand the feature that we are adding telemetry to.
2. Contact the Data Science team to get the full requirements.  This includes:
* The categories that the Data Science team expects data from?
* What are the telemetries the Data Science team expects in each category?
* What type of data for each telemetry?
3. Work with the Data Science team to raise/lower expectations.  Refine the requirements until every telemetry is clearly specified.  This includes:
* Inform the Data Science team which telemetry is not achievable. (if exists)
* Inform the Data Science team other possible telemetry that they might not know about.
* Inform the Data Science team what might not make sense to collection.  (Ex: B always happens when A happens)
* Help the Data Science team collect the best telemetry data possible.
4. Consult with the Glean team if there’s any questions. (Ex: What type of data to use)

## Procedure to follow when implementing a Glean telemetry event
* A full example of adding an event with keys can be found [here](https://github.com/mozilla-mobile/android-components/pull/10837) (Android Components), [here](https://github.com/mozilla-mobile/fenix/pull/20909) (Fenix) and [here](https://github.com/mozilla/glean-annotations/pull/77) (Glean Annotation)
1. Create an event in [metrics.yaml](https://github.com/mozilla-mobile/fenix/blob/master/app/metrics.yaml) and do a project rebuild to generate the event
2. To add feature tags see steps [here](https://github.com/mozilla-mobile/fenix/wiki/Metric-Feature-Tags).
5. Send the event from the proper place in the code with the appropriate generated method (e.g. `GeneratedClassMetrics.generatedEvent.record()`)
6. Create pull requests
7. Submit a data review ([example here](https://github.com/mozilla-mobile/fenix/pull/20909#issuecomment-902119039)).  There's also a [command-line tool for generating Data Review Requests](https://chuttenblog.wordpress.com/2021/09/07/this-week-in-glean-data-reviews-are-important-glean-parser-makes-them-easy/)
8. Update the [metrics.yaml](https://github.com/mozilla-mobile/fenix/blob/master/app/metrics.yaml) with the data review
9. For startup metrics, make sure to [manually test it](https://github.com/mozilla-mobile/fenix/wiki/Test-telemetry-pings)

## Review
See example [here](https://github.com/mozilla-mobile/fenix/pull/20909)
* Add a developer that understands telemetry to review your change.
* Add a developer from the Glean team as reviewer if needed.
* Data review format [here](https://github.com/mozilla/data-review/blob/main/request.md). ([example here](https://github.com/mozilla-mobile/fenix/pull/20909#issuecomment-902119039))

## After Merge
1. Make a note to revisit your telemetry changes when it makes it to beta/release.
* for events, go to Glean dictionary and find the event you want to verify.  Click on the Looker link on the bottom of the page to confirm that the event is being reported.  (For example, for credit_cards.modified, the Glean dictionary link is https://dictionary.telemetry.mozilla.org/apps/fenix/metrics/credit_cards_modified.  On the bottom click on the "credit_cards.modified" link next to Looker to see event count)
* for metrics, create a query (ex: https://sql.telemetry.mozilla.org/queries/82373) to confirm that metric is being reported.
2. Work with the data science team to make sure that they are seeing data that meet their requirements.

## Renewing Expiring Telemetry
See steps [here](https://github.com/mozilla-mobile/fenix/wiki/Creating-a-release-branch#renew-telemetry)