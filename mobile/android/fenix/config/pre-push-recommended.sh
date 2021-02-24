#!/bin/sh

# We recommend you run this as a pre-push hook: to reduce
# review turn-around time, we want all pushes to run tasks
# locally. Using this hook will guarantee your hook gets
# updated as the repository changes.
#
# This hook tries to run as much as possible without taking
# too long.
#
# You can use it by running this command from the project root:
# `ln -s ../../config/pre-push-recommended.sh .git/hooks/pre-push`

# Descriptions for each gradle task below can be found in the
# output of `./gradlew tasks`.

# Prevent push if generated glean docs are not committed.
# A better implementation would make sure these doc updates
# only came from this commit.
./gradlew -q \
        gleanGenerateMetricsDocsForDebug \
        gleanGenerateMetricsSourceForDebug
if git status --porcelain=v1 | grep -q "docs/metrics.md"; then
  echo "
FAIL pre-push hook: generated glean file, docs/metrics.md, has uncommitted changes.
Please commit these files and try again.

This check tries to prevent these generated files from being uncommitted on master.
However, it may fail unintuitively if we're in that state. If this happens often
and is disruptive to your workflow, please notify mcomella so we can improve this
check." >&2
  exit 1
fi

# Run core checks.
./gradlew -q \
        ktlint \
        detekt \
        assembleDebug \
        assembleDebugAndroidTest \
        mozilla-detekt-rules:test \
        mozilla-lint-rules:test \
        testDebug

# Tasks omitted because they take a long time to run:
# - assembling all variants
# - unit test on all variants
# - UI tests
# - android lint (takes a long time to run)
