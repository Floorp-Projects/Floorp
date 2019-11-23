#!/bin/bash

set -e

/etc/init.d/dbus start

exec "${@}"