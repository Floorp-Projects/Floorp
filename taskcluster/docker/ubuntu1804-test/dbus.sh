#!/bin/bash

set -e

/etc/init.d/dbus start 2>&1

exec "${@}"
