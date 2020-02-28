[![Rust][Rust Badge]][Rust CI Link]
[![NotImplemented Counter][NotImplemented Badge]][NotImplemented Search]
[![Fuzzbug days since][Fuzzbug Days Badge]][Fuzzbugs]
[![Fuzzbug open][Fuzzbug Open Badge]][Open Fuzzbugs]

# Metrics

Unlike other branches in this project, this branch is for collecting metrics from the CI. you will
find these files in the `.results` folder. If this branch gets deleted, don't worry. This branch can be auto-generated from the `.metrics`
folder in the main repository.

## Types of data

These are the types of data that this metrics folder tracks.

1) NotImplemented Count
    * counts number of NotImplemented errors in the codebase. This should slowly rundown to zero
    * Updates on every push to master. See [this
        action](https://github.com/mozilla-spidermonkey/jsparagus/tree/master/.github/workflows/ci-push.yml)

2) Days Since last Fuzzbug
    * tracks the last fuzzbug we saw, if it does not exist, return ∞, otherwise return the last date regardless of state.
    * Updates daily, regardless of push. See [this
        action](https://github.com/mozilla-spidermonkey/jsparagus/tree/master/.github/workflows/ci-daily.yml)

3) Fuzzbug open count
    * tracks the number of open fuzzbugs
    * Updates daily, regardless of push. See [this
        action](https://github.com/mozilla-spidermonkey/jsparagus/tree/master/.github/workflows/ci-issues.yml)

4) Percentage of tests passing with SmooshMonkey
    * TODO: tracks the number of tests passing without fallback. We should use the try api for this.
    * Updates daily, regardless of push. See [this
        action](https://github.com/mozilla-spidermonkey/jsparagus/tree/master/.github/workflows/ci-daily.yml)


5) Percentage of JS compilable with SmooshMonkey
    * TODO: see comment about writing bytes to a file in [this repo](https://github.com/nbp/seqrec)
    * implementation is dependant on how we get the data. We need a robust solution for importing this data.

[Rust Badge]: https://github.com/mozilla-spidermonkey/jsparagus/workflows/Rust/badge.svg
[Rust CI Link]: https://github.com/mozilla-spidermonkey/jsparagus/actions?query=branch%3Amaster
[NotImplemented Badge]: https://img.shields.io/endpoint?url=https%3A%2F%2Fraw.githubusercontent.com%2Fmozilla-spidermonkey%2Fjsparagus%2Fci_results%2F.metrics%2Fbadges%2Fnot-implemented.json
[NotImplemented Search]: https://github.com/mozilla-spidermonkey/jsparagus/search?q=notimplemented&unscoped_q=notimplemented
[Fuzzbug days Badge]: https://img.shields.io/endpoint?url=https%3A%2F%2Fraw.githubusercontent.com%2Fmozilla-spidermonkey%2Fjsparagus%2Fci_results%2F.metrics%2Fbadges%2Fsince-last-fuzzbug.json
[Fuzzbug Open Badge]: https://img.shields.io/endpoint?url=https%3A%2F%2Fraw.githubusercontent.com%2Fmozilla-spidermonkey%2Fjsparagus%2Fci_results%2F.metrics%2Fbadges%2Fopen-fuzzbug.json
[Fuzzbugs]: https://github.com/mozilla-spidermonkey/jsparagus/issues?utf8=%E2%9C%93&q=label%3AlibFuzzer+
[Open Fuzzbugs]: https://github.com/mozilla-spidermonkey/jsparagus/labels/libFuzzer
