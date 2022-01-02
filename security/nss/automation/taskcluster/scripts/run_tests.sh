#!/usr/bin/env bash

source $(dirname "$0")/tools.sh

# Fetch artifact if needed.
fetch_dist

# Run tests.
cd nss/tests && ./all.sh
