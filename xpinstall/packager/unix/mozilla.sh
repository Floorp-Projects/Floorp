#!/bin/sh

MOZILLA_FIVE_HOME=/usr/lib/mozilla
export MOZILLA_FIVE_HOME
exec /usr/bin/mozilla-bin $*
