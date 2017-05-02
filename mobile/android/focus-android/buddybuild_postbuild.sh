#!/usr/bin/env bash
set -e # Exit (and fail) immediately if any command in this scriptfails

# buddybuild doesn't seem to offer any direct way of running findbugs.
# findbugs is run as part of |gradle build| and |gradle| check, but those
# aren't run directly in buddybuild.
./gradlew findbugs

./gradlew jacocoTestReport
bash <(curl -s https://codecov.io/bash) -t $CODECOV_TOKEN
