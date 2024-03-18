#!/bin/sh

set -x -e -v

VERSION="$1"

BASE_URL=https://plugins.gradle.org/m2/gradle/plugin/com/jetbrains/python/gradle-python-envs

mkdir -p "${UPLOAD_DIR}"
wget --no-parent --recursive --execute robots=off "${BASE_URL}/${VERSION}/"
tar caf "${UPLOAD_DIR}/gradle-python-envs-${VERSION}.tar.zst" plugins.gradle.org
