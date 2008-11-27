#!/bin/bash -e
# -*- Mode: Shell-script; tab-width: 4; indent-tabs-mode: nil; -*-
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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2006.
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Bob Clary <bob@bclary.com>
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

source $TEST_DIR/bin/library.sh
source $TEST_DIR/bin/set-build-env.sh $@

case $product in
    firefox|thunderbird|fennec)

        if ! $buildbash $bashlogin -c "cd $BUILDTREE/mozilla; make -f client.mk clean" 2>&1; then
            error "during client.mk clean" $LINENO
        fi
        ;;

    js)
        if [[ -e "$BUILDTREE/mozilla/js/src/configure.in" ]]; then
            # use the new fangled autoconf build environment for spidermonkey

            # recreate the OBJ directories to match the old naming standards
            TEST_JSDIR=${TEST_JSDIR:-$TEST_DIR/tests/mozilla.org/js}
            source $TEST_JSDIR/config.sh

            mkdir -p "$BUILDTREE/mozilla/js/src/$JS_OBJDIR"

            if [[ ! -e "$BUILDTREE/mozilla/js/src/configure" ]]; then

                if findprogram autoconf-2.13; then
                    AUTOCONF=autoconf-2.13
                elif findprogram autoconf213; then
                    AUTOCONF=autoconf213
                else
                    error "autoconf 2.13 not detected"
                fi

                cd "$BUILDTREE/mozilla/js/src"
                eval "$AUTOCONF" 

            fi

            cd "$BUILDTREE/mozilla/js/src/$JS_OBJDIR"

            if [[ -e "Makefile" ]]; then
                if ! $buildbash $bashlogin -c "cd $BUILDTREE/mozilla/js/src/$JS_OBJDIR; make clean" 2>&1; then
                    error "during js/src clean" $LINENO
                fi
            fi

        elif [[ -e "$BUILDTREE/mozilla/js/src/Makefile.ref" ]]; then

            if ! $buildbash $bashlogin -c "cd $BUILDTREE/mozilla/js/src/editline; make -f Makefile.ref clean" 2>&1; then
                error "during editline clean" $LINENO
            fi

            if ! $buildbash $bashlogin -c "cd $BUILDTREE/mozilla/js/src; make -f Makefile.ref clean" 2>&1; then
                error "during SpiderMonkey clean" $LINENO
            fi
        else
            error "Neither Makefile.ref or autoconf builds available"
        fi
        ;;
esac
