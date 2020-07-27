=======
History
=======

Unreleased
----------

1.18.3 (2020-02-24)
-------------------

* Dropped the 'inflection' dependency.
* Constrained the 'zipp' and 'MarkupSafe' transitive dependencies to versions that
  support Python 3.5.

1.18.2 (2020-02-14)
-------------------

* BUGFIX: Fix rendering of first element of reason list.

1.18.1 (2020-02-14)
-------------------

* BUGFIX: Reason codes are displayed in markdown output for built-in pings as
  well.
* BUGFIX: Reason descriptions are indented correctly in markdown output.
* BUGFIX: To avoid a compiler error, the @JvmName annotation isn't added to
  private members.

1.18.0 (2020-02-13)
-------------------

* **Breaking Change (Java API)** Have the metrics names in Java match the names in Kotlin.
  See [Bug 1588060](https://bugzilla.mozilla.org/show_bug.cgi?id=1588060).
* The reasons a ping are sent are now included in the generated markdown documentation.

1.17.3 (2020-02-05)
-------------------

* BUGFIX: The version of Jinja2 now specifies < 3.0, since that version no
  longer supports Python 3.5.

1.17.2 (2020-02-05)
-------------------

* BUGFIX: Fixes an import error in generated Kotlin code.

1.17.1 (2020-02-05)
-------------------

* BUGFIX: Generated Swift code now includes `import Glean`, unless generating
  for a Glean-internal build.

1.17.0 (2020-02-03)
-------------------

* Remove default schema URL from `validate_ping`
* Make `schema` argument required for CLI
* BUGFIX: Avoid default import in Swift code for Glean itself
* BUGFIX: Restore order of fields in generated Swift code

1.16.0 (2020-01-15)
-------------------

* Support for `reason` codes on pings was added.

1.15.6 (2020-02-06)
-------------------

* BUGFIX: The version of Jinja2 now specifies < 3.0, since that version no
  longer supports Python 3.5 (backported from 1.17.3).

1.15.5 (2019-12-19)
-------------------

* BUGFIX: Also allow the legacy name `all_pings` for `send_in_pings` parameter on metrics

1.15.4 (2019-12-19)
-------------------

* BUGFIX: Also allow the legacy name `all_pings`

1.15.3 (2019-12-13)
-------------------

* Add project title to markdown template.
* Remove "Sorry about that" from markdown template.
* BUGFIX: Replace dashes in variable names to force proper naming

1.15.2 (2019-12-12)
-------------------

* BUGFIX: Use a pure Python library for iso8601 so there is no compilation required.

1.15.1 (2019-12-12)
-------------------

* BUGFIX: Add some additional ping names to the non-kebab-case allow list.

1.15.0 (2019-12-12)
-------------------

* Restrict new pings names to be kebab-case and change `all_pings` to `all-pings`

1.14.0 (2019-12-06)
-------------------

* glean_parser now supports Python versions 3.5, 3.6, 3.7 and 3.8.

1.13.0 (2019-12-04)
-------------------

* The `translate` command will no longer clear extra files in the output directory.
* BUGFIX: Ensure all newlines in comments are prefixed with comment markers
* BUGFIX: Escape Swift keywords in variable names in generated code
* Generate documentation for pings that are sent if empty

1.12.0 (2019-11-27)
-------------------

* Reserve the `deletion_request` ping name
* Added a new flag `send_if_empty` for pings

1.11.0 (2019-11-13)
-------------------

* The `glinter` command now performs `yamllint` validation on registry files.

1.10.0 (2019-11-11)
-------------------

* The Kotlin linter `detekt` is now run during CI, and for local
  testing if installed.

* Python 3.8 is now tested in CI (in addition to Python 3.7).
  Using `tox` for this doesn't work in modern versions of CircleCI, so
  the `tox` configuration has been removed.

* `yamllint` has been added to test the YAML files on CI.

* ⚠ Metric types that don't yet have implementations in glean-core have been
  removed. This includes `enumeration`, `rate`, `usage`, and `use_counter`, as
  well as many labeled metrics that don't exist.

1.9.5 (2019-10-22)
------------------

* Allow a Swift lint for generated code

* New lint: Restrict what metric can go into the 'baseline' ping

* New lint: Warn for slight misspellings in ping names

* BUGFIX: change Labeled types labels from lists to sets.

1.9.4 (2019-10-16)
------------------

* Use lists instead of sets in Labeled types labels to ensure that
  the order of the labels passed to the `metrics.yaml` is kept.

* `glinter` will now check for duplicate labels and error if there are any.

1.9.3 (2019-10-09)
------------------

* Add labels from Labeled types to the Extra column in the Markdown template.

1.9.2 (2019-10-08)
------------------

* BUGFIX: Don't call `is_internal_metric` on `Ping` objects.

1.9.1 (2019-10-07)
------------------

* Don't include Glean internal metrics in the generated markdown.

1.9.0 (2019-10-04)
------------------

* Glinter now warns when bug numbers (rather than URLs) are used.

* BUGFIX: add `HistogramType` and `MemoryUnit` imports in Kotlin generated code.

1.8.4 (2019-10-02)
------------------

* Removed unsupported labeled metric types.

1.8.3 (2019-10-02)
------------------

* Fix indentation for generated Swift code

1.8.2 (2019-10-01)
------------------

* Created labeled metrics and events in Swift code and wrap it in a configured namespace

1.8.1 (2019-09-27)
------------------

* BUGFIX: `memory_unit` is now passed to the Kotlin generator.

1.8.0 (2019-09-26)
------------------

* A new parser config, `do_not_disable_expired`, was added to turn off the
  feature that expired metrics are automatically disabled. This is useful if you
  want to retain the disabled value that is explicitly in the `metrics.yaml`
  file.

* `glinter` will now report about superfluous `no_lint` entries.

1.7.0 (2019-09-24)
------------------

* A "`glinter`" tool is now included to find common mistakes in metric naming and setup.
  This check is run during `translate` and warnings will be displayed.
  ⚠ These warnings will be treated as errors in a future revision.

1.6.1 (2019-09-17)
------------------

* BUGFIX: `GleanGeckoMetricsMapping` must include `LabeledMetricType` and `CounterMetricType`.

1.6.0 (2019-09-17)
------------------

* NEW: Support for outputting metrics in Swift.

* BUGFIX: Provides a helpful error message when `geckoview_datapoint` is used on an metric type that doesn't support GeckoView exfiltration.

* Generate a lookup table for Gecko categorical histograms in `GleanGeckoMetricsMapping`.

* Introduce a 'Swift' output generator.

1.4.1 (2019-08-28)
------------------

* Documentation only.

1.4.0 (2019-08-27)
------------------

* Added support for generating markdown documentation from `metrics.yaml` files.

1.3.0 (2019-08-22)
------------------

* `quantity` metric type has been added.

1.2.1 (2019-08-13)
------------------

* BUGFIX: `includeClientId` was not being output for PingType.

1.2.0 (2019-08-13)
------------------

* `memory_distribution` metric type has been added.

* `custom_distribution` metric type has been added.

* `labeled_timespan` is no longer an allowed metric type.

1.1.0 (2019-08-05)
------------------

* Add a special `all_pings` value to `send_in_pings`.

1.0.0 (2019-07-29)
------------------

* First release to start following strict semver.

0.1.0 (2018-10-15)
------------------

* First release on PyPI.
