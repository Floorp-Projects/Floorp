#!/bin/sh

./gradlew -q \
        ktlint \
        detekt \
        app:assembleX86GreenfieldDebug
