#!/bin/bash
export TMPDIR=$XDG_CACHE_HOME/tmp
exec /app/lib/firefox/firefox --name org.mozilla.firefox "$@"
