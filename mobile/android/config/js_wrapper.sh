#! /bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Wrapper for running SpiderMonkey js shell in automation with correct
# LD_LIBRARY_PATH.

# We don't have a reference to topsrcdir at this point, but we are invoked as
# "$topsrcdir/mobile/android/config/js_wrapper.sh" so we can extract topsrcdir
# from $0.
topsrcdir=`cd \`dirname $0\`/../../..; pwd`

JS_BINARY="$topsrcdir/jsshell/js"

LD_LIBRARY_PATH="$topsrcdir/jsshell${LD_LIBRARY_PATH+:$LD_LIBRARY_PATH}"
export LD_LIBRARY_PATH

# Pass through all arguments and exit with status from js shell.
exec "$JS_BINARY" "$@"
