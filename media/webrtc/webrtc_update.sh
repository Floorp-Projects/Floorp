#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# First, get a new copy of the tree to play with
# They both want to be named 'trunk'...
mkdir webrtc_update
cd webrtc_update

# Note: must be in trunk; won't work with --name (error during sync)
gclient config --name trunk http://webrtc.googlecode.com/svn/trunk/peerconnection
gclient sync --force
if [ $2 ] ; then
if [ $3 ] ; then
    echo
else
    sed -i -e "s/\"webrtc_revision\":.*,/\"webrtc_revision\": \"$1\",/" -e "s/\"libjingle_revision\":.*,/\"libjingle_revision\": \"$2\",/" trunk/DEPS
    gclient sync --force
fi
fi

if [ $3 ] ; then
echo "Updating from $3..."
rm -rf trunk/third_party/webrtc
mkdir trunk/src
cp -a $3/webrtc/* trunk/src
fi

cd trunk

export date=`date`
export revision=`svnversion -n`
if [ $1 ] ; then
  echo "WebRTC revision overridden from $revision to $1" 
  export revision=$1
else
  echo "WebRTC revision = $revision"
fi

cd ../../media/webrtc

# safety - make it easy to find our way out of the forest
hg tag -f -l old-tip

# Ok, now we have a copy of the source.  See what changed
# (webrtc-import-last is a bookmark)
# FIX! verify no changes in webrtc!
hg update --clean webrtc-import-last

rm -rf trunk
mv ../../webrtc_update/trunk trunk
mv -f ../../webrtc_update/.g* .
rmdir ../../webrtc_update
(hg addremove --exclude "**webrtcsessionobserver.h" --exclude "**webrtcjson.h" --exclude "**.svn" --exclude "**.git" --exclude "**.pyc" --exclude "**.yuv" --similarity 70 --dry-run trunk; hg status -m; echo "************* Please look at the filenames found by rename and see if they match!!! ***********" ) | less

# FIX! Query user about add-removes better!!
echo "Waiting 30 seconds - Hit ^C now to stop addremove and commit!"
sleep 30  # let them ^C

# Add/remove files, detect renames
hg addremove --exclude "**webrtcsessionobserver.h" --exclude "**webrtcjson.h" --exclude "**.svn" --exclude "**.git" --exclude "**.pyc" --exclude "**.yuv" --similarity 70 trunk
hg status -a -r >/tmp/changed_webrtc_files

# leave this for the user for now until we're comfortable it works safely

# Commit the vendor branch
echo "Commit, merge and push to server - cut and paste"
echo "You probably want to do these from another shell so you can look at these"
hg commit -m "Webrtc import $revision"
# webrtc-import-last is auto-updated (bookmark)

#echo ""
#hg update --clean webrtc-pending
#hg merge -r webrtc-import-last
#hg commit -m "merge latest import to pending, $revision"
# webrtc-pending is auto-updated (bookmark)

echo ""
echo "hg update --clean webrtc-trim"
echo "hg merge -r webrtc-import-last"
echo "hg commit -m 'merge latest import to trim, $revision'"
# webrtc-trim is auto-updated (bookmark)

# commands to pull - never do more than echo them for the user
echo ""
echo "Here's how to pull this update into the mozilla repo:"
echo "cd your_tree"
echo "hg qpop -a"
echo "hg pull --bookmark webrtc-trim path-to-webrtc-import-repo"
echo "hg merge"
echo "echo '#define WEBRTC_SVNVERSION $revision' >media/webrtc/webrtc_version.h"
echo "echo '#define WEBRTC_PULL_DATE \"$date\"' >>media/webrtc/webrtc_version.h"
echo "hg commit -m 'Webrtc updated to $revision; pull made on $date'"
echo ""
echo "Please check for added/removed/moved .gyp/.gypi files, and if so update EXTRA_CONFIG_DEPS in client.mk!"
echo "possible gyp* file changes:"
grep gyp /tmp/changed_webrtc_files
echo ""
echo "Once you feel safe:"
echo "hg push"
