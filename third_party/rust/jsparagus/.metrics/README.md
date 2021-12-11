[![Rust][Rust Badge]][Rust CI Link]
[![NotImplemented Counter][NotImplemented Badge]][NotImplemented Search]
[![Fuzzbug days since][Fuzzbug Days Badge]][Fuzzbugs]
[![Fuzzbug open][Fuzzbug Open Badge]][Open Fuzzbugs]

# Metrics

This is the metrics directory. It follows the evolution of the repository separately from the
repostory. You can find the actual metrics in the
[`ci-results`](https://github.com/mozilla-spidermonkey/jsparagus/tree/ci-results) branch of the jsparagus project. This branch is automatically generated using the `create-ci-branch.sh` script found in this directory. If there are issues with your fork, you can remove the `ci-results` branch, and the ci will automatically rerun the `create-ci-branch` script to reset it. Do not push manula data to this repository, it will be lost.

If you find that the `ci-results` branch has disappeared or been corrupted somehow, you can reset it by deleting it and recreating it.

```
git branch -D ci-results
cd .metrics
./create-ci-branch.sh
```

The `create-ci-branch.sh` file creates the branch, prepares it, and populates it with data from the past.

## Making your own metrics
Make sure you do not use data that can not be automatically recovered. We cannot rely on the `ci-results` branch always being present, therefore anything that you write must be recoverable on its own, either by relying on external APIs or through some other mechanism.

Please update this README if you make any changes.

## Types of CI Actions
These actions are all found in the `.github/workflows` directory

1) `Rust.yml` - Run on Pull Request
* runs every time there is a push to master, use for any metrics that are development related. Examples include linting, testing, etc.
2) `ci-push.yml` - Run on Push to `master`
* runs on self contained metrics. An example is the number of `NotImplemented` errors in the codebase. This does not depend on anything external
3) `ci-daily.yml` - Run Daily
* a cron task that runs daily. Useful for metrics that need daily updates
4) `ci-issue.yml` - Run on issue open
* runs each time an issue is opened. Good for tracking types of issues.


## Types of data

These are the types of data that this metrics folder tracks.

1) Rust Passing
    * Ensures our internal tests are passing
    * Updates on every pull request to master. See [this
        action](https://github.com/mozilla-spidermonkey/jsparagus/tree/master/.github/workflows/rust.yml)

2) NotImplemented Count
    * counts number of NotImplemented errors in the codebase. This should slowly rundown to zero
    * Updates on every push to master. See [this
        action](https://github.com/mozilla-spidermonkey/jsparagus/tree/master/.github/workflows/ci-push.yml)

3) Days Since last Fuzzbug
    * tracks the last fuzzbug we saw, if it does not exist, return ∞, otherwise return the last date regardless of state.
    * Updates daily, regardless of push. See [this
        action](https://github.com/mozilla-spidermonkey/jsparagus/tree/master/.github/workflows/ci-daily.yml)

4) Fuzzbug open count
    * tracks the number of open fuzzbugs
    * Updates on issue open. See [this action](https://github.com/mozilla-spidermonkey/jsparagus/.github/workflows/ci-issues.yml)

5) Percentage of tests passing with SmooshMonkey
    * TODO: tracks the number of tests passing without fallback. We should use the try api for this.
    * Updates daily, regardless of push. See [this
        action](https://github.com/mozilla-spidermonkey/jsparagus/tree/master/.github/workflows/ci-daily.yml)

6) Percentage of JS compilable with SmooshMonkey
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
