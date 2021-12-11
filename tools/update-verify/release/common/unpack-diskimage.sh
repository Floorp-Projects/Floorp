#!/bin/bash
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the installdmg.sh script from taols utilities
#
# The Initial Developer of the Original Code is
# Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2009
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Chris AtLee <catlee@mozilla.com>
#  Robert Kaiser <kairo@kairo.at>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

# Unpack a disk image to a specified target folder
#
# Usage: unpack-diskimage <image_file>
#                         <mountpoint>
#                         <target_path>

DMG_PATH=$1
MOUNTPOINT=$2
TARGETPATH=$3
LOGFILE=unpack.output

# How long to wait before giving up waiting for the mount to fininsh
TIMEOUT=90

# If the mount point already exists, then the previous run may not have cleaned
# up properly.  We should try to umount and remove the its directory.
if [ -d $MOUNTPOINT ]; then
    echo "$MOUNTPOINT already exists, trying to clean up"
    hdiutil detach $MOUNTPOINT -force
    rm -rdfv $MOUNTPOINT
fi

# Install an on-exit handler that will unmount and remove the '$MOUNTPOINT' directory
trap "{ if [ -d $MOUNTPOINT ]; then hdiutil detach $MOUNTPOINT -force; rm -rdfv $MOUNTPOINT; fi; }" EXIT

mkdir -p $MOUNTPOINT

hdiutil attach -verbose -noautoopen -mountpoint $MOUNTPOINT "$DMG_PATH" &> $LOGFILE
# Wait for files to show up
# hdiutil uses a helper process, diskimages-helper, which isn't always done its
# work by the time hdiutil exits.  So we wait until something shows up in the
# mount point directory.
i=0
while [ "$(echo $MOUNTPOINT/*)" == "$MOUNTPOINT/*" ]; do
    if [ $i -gt $TIMEOUT ]; then
        echo "No files found, exiting"
        exit 1
    fi
    sleep 1
    i=$(expr $i + 1)
done
# Now we can copy everything out of the $MOUNTPOINT directory into the target directory
rsync -av $MOUNTPOINT/* $MOUNTPOINT/.DS_Store $MOUNTPOINT/.background $MOUNTPOINT/.VolumeIcon.icns $TARGETPATH/ > $LOGFILE
# sometimes hdiutil fails with "Resource busy"
hdiutil detach $MOUNTPOINT || { sleep  10; \
    if [ -d $MOUNTPOINT ]; then hdiutil detach $MOUNTPOINT -force; fi; }
i=0
while [ "$(echo $MOUNTPOINT/*)" != "$MOUNTPOINT/*" ]; do
    if [ $i -gt $TIMEOUT ]; then
        echo "Cannot umount, exiting"
        exit 1
    fi
    sleep 1
    i=$(expr $i + 1)
done
rm -rdf $MOUNTPOINT
