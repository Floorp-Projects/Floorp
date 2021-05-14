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
