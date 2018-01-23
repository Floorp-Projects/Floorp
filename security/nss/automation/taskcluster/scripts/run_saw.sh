#!/usr/bin/env bash

source $(dirname "$0")/tools.sh

# Fetch artifact if needed.
fetch_dist

# Run SAW.
saw "nss/automation/saw/$1.saw"
