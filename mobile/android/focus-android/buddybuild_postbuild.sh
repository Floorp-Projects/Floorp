#!/usr/bin/env bash
set -e # Exit (and fail) immediately if any command in this scriptfails

# Check that our APK files are not bigger than they should be
tools/metrics/apk_size.sh

# buddybuild modifies our buildscripts and sources (this is partly to enable
# their SDK, and partly to allow selecting flavours in the BuddyBuild UI).
# We don't know where the Buddybuild SDK lives, which causes gradle builds
# to be broken (due to the SDK dependency injected into FocusApplication),
# it's easiest just to revert to a clean source state here:
git reset --hard

# buddybuild doesn't seem to offer any direct way of running findbugs.
# findbugs is run as part of |gradle build| and |gradle| check, but those
# aren't run directly in buddybuild.
./gradlew findbugs

./gradlew jacocoTestReport
bash <(curl -s https://codecov.io/bash) -t $CODECOV_TOKEN
