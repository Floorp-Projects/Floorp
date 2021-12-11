Vision
======

The `mozperftest` project was created with the intention to replace all
existing performance testing frameworks that exist in the mozilla central
source tree with a single one, and make performance tests a standardized, first-class
citizen, alongside mochitests and xpcshell tests.

We want to give the ability to any developer to write performance tests in
their component, both locally and in the CI, exactly like how they would do with
`xpcshell` tests and `mochitests`.

Historically, we have `Talos`, that provided a lot of different tests, from
micro-benchmarks to page load tests. From there we had `Raptor`, that was a
fork of Talos, focusing on page loads only. Then `mach browsertime` was added,
which was a wrapper around the `browsertime` tool.

All those frameworks besides `mach browsertime` were mainly focusing on working
well in the CI, and were hard to use locally. `mach browsertime` worked locally but
not on all platforms and was specific to the browsertime framework.

`mozperftest` currently provides the `mach perftest` command, that will scan
for all tests that are declared in ini files such as
https://searchfox.org/mozilla-central/source/netwerk/test/perf/perftest.ini and
registered under **PERFTESTS_MANIFESTS** in `moz.build` files such as
https://searchfox.org/mozilla-central/source/netwerk/test/moz.build#17

If you launch `./mach perftest` without any parameters, you will get a full list
of available tests, and you can pick and run one. Adding `--push-to-try` will
run it on try.

The framework loads perf tests and read its metadata, that can be declared
within the test. We have a parser that is currently able to recognize and load
**xpcshell** tests and **browsertime** tests, and a runner for each one of those.

But the framework can be extended to support more formats. We would like to add
support for **jsshell** and any other format we have in m-c.

A performance test is a script that perftest runs, and that returns metrics we
can use. Right now we consume those metrics directly in the console, and
also in perfherder, but other formats could be added. For instance, there's
a new **influxdb** output that has been added, to push the data in an **influxdb**
time series database.

What is important is to make sure performance tests belong to the component it's
testing in the source tree. We've learned with Talos that grouping all performance
tests in a single place is problematic because there's no sense of ownership from
developers once it's added there. It becomes the perf team problem. If the tests
stay in each component alongside mochitests and xpcshell tests, the component
maintainers will own and maintain it.


Next steps
----------

We want to rewrite all Talos and Raptor tests into perftest. For Raptor, we need
to have the ability to use proxy records, which is a work in progress. From there,
running a **raptor** test will be a simple, one liner browsertime script.

For Talos, we'll need to refactor the existing micro-benchmarks into xpchsell tests,
and if that does not suffice, create a new runner.

For JS benchmarks, once the **jsshell** runner is added into perftest, it will be
straightforward.


