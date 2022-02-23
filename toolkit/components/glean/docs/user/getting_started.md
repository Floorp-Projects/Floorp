# Getting Started with Firefox on Glean (FOG)

This documentation is designed to be helpful to those who are
* New to data collection in Firefox Desktop,
* Experienced with data collection in Firefox Desktop, but not the Glean kind
* Those who are just interested in a refresher.

## What is FOG?

Firefox on Glean (FOG) is the library that brings
[the Glean SDK](https://mozilla.github.io/glean/book/index.html),
Mozilla's modern data collection system,
to Firefox Desktop.

FOG's code is in `toolkit/components/glean` and is considered part of the
`Toolkit :: Telemetry` [module][modules].
Bugs against FOG can be [filed][file-fog-bugs]
in Bugzilla in the `Toolkit` product and the `Telemetry` component.
(No bugs about adding new instrumentation, please.
You can file those in the components that you want instrumented.)
You can find folks who can help answer your questions about FOG in
* [#glean:mozilla.org](https://chat.mozilla.org/#/room/#glean:mozilla.org)
* [#telemetry:mozilla.org](https://chat.mozilla.org/#/room/#telemetry:mozilla.org)
* Slack#data-help

On top of the usual things Glean embedders supply
(user engagement monitoring, network upload configuration, data upload preference watching, ...)
FOG supplies Firefox-Desktop-specific things:
* Privileged JS API
* C++ API
* IPC
* Test Preferences
* Support for `xpcshell`, browser-chrome mochitests, GTests, and rusttests
* `about:glean`
* ...and more.

## What do I need to know about Glean?

You use the APIs supplied by the Glean SDK to instrument Mozilla projects.

The unit of instrumentation is the **metric**.
Recording the number of times a user opens a new tab? That's a metric.
Timing how long each JS garbage collector pass takes? Also a metric.

Glean has documentation about
[how to add a new metric][add-a-metric]
that you should follow to learn how to add a metric to instrument Firefox Desktop.
There are some [peculiarities specific to Firefox Desktop](new_definitions_file)
that you'll wish to review as well.
Don't forget to get [Data Collection Review][data-review]
for any new or expanded data collections in mozilla projects.

By adding a metric you've told the Glean SDK what shape of instrumentation you want.
And by using the metric's APIs to instrument your code,
you've put your interesting data into that metric.
But how does the data leave Firefox Desktop and make it to Mozilla's Data Pipeline?

Batches of related metrics are collected into **pings**
which are submitted according to their specific schedules.
If you don't say otherwise, any non-`event`-metric will be sent in the
[built-in Glean "metrics" ping][metrics-ping] about once a day.
(`event` metrics are sent in [the "events" ping][events-ping]
more frequently than that).

With data being sent to Mozilla's Data Pipeline, how do you analyse it?

That's an impossible question to answer completely without knowing a _lot_ about what questions you want to answer.
However, in general, if you want to see what data is being collected by your instrumentation,
[go to its page in Glean Dictionary][glean-dictionary]
and you'll find links and information there about how to proceed.

## Where do I learn more?

Here in the [FOG User Documentation](./index) you will find FOG-specific details like
[how to write instrumentation tests](instrumentation_tests), or
[how to use Glean APIs to mirror data to Telemetry](gifft).

Most of what you should have to concern yourself with, as an instrumentor,
is documented in [the Book of Glean](https://mozilla.github.io/glean/book/index.html).
Such as its [illuminating glossary][glean-glossary],
the [list of all metric types][metrics-types],
or the index of our long-running blog series [This Week in Glean][twig-index].

And for anything else you need help with, please find us in
[#glean:mozilla.org](https://chat.mozilla.org/#/room/#glean:mozilla.org).
We'll be happy to help you learn more about FOG and Glean.

[add-a-metric]: https://mozilla.github.io/glean/book/user/metrics/adding-new-metrics.html
[metrics-ping]: https://mozilla.github.io/glean/book/user/pings/metrics.html
[events-ping]: https://mozilla.github.io/glean/book/user/pings/events.html
[modules]: https://wiki.mozilla.org/Modules/All
[data-review]: https://wiki.mozilla.org/Data_Collection
[glean-dictionary]: https://dictionary.telemetry.mozilla.org/
[glean-glossary]: https://mozilla.github.io/glean/book/appendix/glossary.html
[twig-index]: https://mozilla.github.io/glean/book/appendix/twig.html
[metrics-types]: https://mozilla.github.io/glean/book/reference/metrics/index.html
[file-fog-bugs]: https://bugzilla.mozilla.org/enter_bug.cgi?product=Toolkit&component=Telemetry
