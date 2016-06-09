#!/bin/bash -vex

gecko_dir=$1
test -d $gecko_dir
test -n "$TOOLTOOL_CACHE"
test -n "$TOOLTOOL_MANIFEST"
test -n "$TOOLTOOL_REPO"
test -n "$TOOLTOOL_REV"

tc-vcs checkout $gecko_dir/tooltool $TOOLTOOL_REPO $TOOLTOOL_REPO $TOOLTOOL_REV

(cd $gecko_dir; python $gecko_dir/tooltool/tooltool.py --url https://api.pub.build.mozilla.org/tooltool/ -m $gecko_dir/$TOOLTOOL_MANIFEST fetch -c $TOOLTOOL_CACHE)

