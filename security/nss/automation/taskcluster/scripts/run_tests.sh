#!/usr/bin/env bash

source $(dirname $0)/tools.sh

if [ $(id -u) = 0 ]; then
    # Stupid Docker.
    echo "127.0.0.1 localhost.localdomain" >> /etc/hosts

    # Drop privileges by re-running this script.
    exec su worker $0
fi

# Fetch artifact if needed.
fetch_dist

# Run tests.
cd nss/tests && ./all.sh
