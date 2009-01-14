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
        cd $BUILDTREE/mozilla

        if ! $buildbash $bashlogin -c "cd $BUILDTREE/mozilla; make -f client.mk build" 2>&1; then

            if [[ -z "$TEST_FORCE_CLOBBER_ON_ERROR" ]]; then
                error "error during build" $LINENO
            else
                echo "error occured during build. attempting a clobber build" $LINENO
                if ! $buildbash $bashlogin -c "cd $BUILDTREE/mozilla; make -f client.mk distclean" 2>&1; then
                    error "error during forced clobber" $LINENO
                fi
                if ! $buildbash $bashlogin -c "cd $BUILDTREE/mozilla; make -f client.mk build" 2>&1; then
                    error "error during forced build" $LINENO
                fi
            fi
        fi

        case "$OSID" in
            mac) 
                if [[ "$buildtype" == "debug" ]]; then
                    if [[ "$product" == "firefox" ]]; then
                        executablepath=$product-$buildtype/dist/FirefoxDebug.app/Contents/MacOS
                    elif [[ "$product" == "thunderbird" ]]; then
                        executablepath=$product-$buildtype/dist/ThunderbirdDebug.app/Contents/MacOS
                    elif [[ "$product" == "fennec" ]]; then
                        executablepath=$product-$buildtype/dist/FennecDebug.app/Contents/MacOS
                    fi
                else
                    if [[ "$product" == "firefox" ]]; then
                        executablepath=$product-$buildtype/dist/Firefox.app/Contents/MacOS
                    elif [[ "$product" == "thunderbird" ]]; then
                        executablepath=$product-$buildtype/dist/Thunderbird.app/Contents/MacOS
                    elif [[ "$product" == "fennec" ]]; then
                        executablepath=$product-$buildtype/dist/Fennec.app/Contents/MacOS
                    fi
                fi
                ;;
            linux)
                executablepath=$product-$buildtype/dist/bin
                ;;
        esac

        if [[ "$OSID" != "nt" && "$product" != "fennec" ]]; then
            #
            # patch unix-like startup scripts to exec instead of 
            # forking new processes
            #
            executable=`get_executable $product $branch $executablepath`

            executabledir=`dirname $executable`

            # patch to use exec to prevent forked processes
            cd "$executabledir"
            if [ -e "$product" ]; then
                echo "$SCRIPT: patching $product"
                cp $TEST_DIR/bin/$product.diff .
                patch -N -p0 < $product.diff
            fi
            if [ -e run-mozilla.sh ]; then
                echo "$SCRIPT: patching run-mozilla.sh"
                cp $TEST_DIR/bin/run-mozilla.diff .
                patch -N -p0 < run-mozilla.diff
            fi
        fi
        ;;
    js)

        if [[ -e "$BUILDTREE/mozilla/js/src/configure.in" ]]; then

            # use the new fangled autoconf build environment for spidermonkey

            # recreate the OBJ directories to match the old naming standards
            TEST_JSDIR=${TEST_JSDIR:-$TEST_DIR/tests/mozilla.org/js}
            source $TEST_JSDIR/config.sh

            mkdir -p "$BUILDTREE/mozilla/js/src/$JS_OBJDIR"

            # run autoconf everytime instead of only when configure
            # needs to be initially generated in order to pick up
            # configure.in changes.

            if findprogram autoconf-2.13; then
                AUTOCONF=autoconf-2.13
            elif findprogram autoconf213; then
                AUTOCONF=autoconf213
            else
                error "autoconf 2.13 not detected"
            fi

            cd "$BUILDTREE/mozilla/js/src"
            eval "$AUTOCONF" 

            cd "$BUILDTREE/mozilla/js/src/$JS_OBJDIR"

            if [[ -e "Makefile" ]]; then
                if ! $buildbash $bashlogin -c "cd $BUILDTREE/mozilla/js/src/$JS_OBJDIR; make clean" 2>&1; then
                    error "during js/src clean" $LINENO
                fi
            fi

            # XXX: Todo
            # This reproduces the limited approach which previously existed with Makefile.ref but
            # does not provide the full functionality provided by the available configure options.
            # Ideally, it would be good to use a mozconfig approach (if available) that would generate
            # the necessary configure command line arguments. This would provide the generality to
            # specify arbitrary configure options.
            #

            if [[ "$buildtype" == "debug" ]]; then
                if ! $buildbash $bashlogin -c "cd $BUILDTREE/mozilla/js/src/$JS_OBJDIR; ../configure --prefix=$BUILDTREE/mozilla/js/src/$JS_OBJDIR  --disable-optimize --enable-debug"; then
                    error "during js/src/$JS_OBJDIR configure" $LINENO
                fi
            else
                if ! $buildbash $bashlogin -c "cd $BUILDTREE/mozilla/js/src/$JS_OBJDIR; ../configure --prefix=$BUILDTREE/mozilla/js/src/$JS_OBJDIR  --enable-optimize --disable-debug"; then
                    error "during js/src/$JS_OBJDIR configure" $LINENO
                fi
            fi

            if ! $buildbash $bashlogin -c "cd $BUILDTREE/mozilla/js/src/$JS_OBJDIR; make" 2>&1; then
                error "during js/src build" $LINENO
            fi

            if ! $buildbash $bashlogin -c "cd $BUILDTREE/mozilla/js/src/$JS_OBJDIR; make install" 2>&1; then
                error "during js/src install" $LINENO
            fi

        elif [[ -e "$BUILDTREE/mozilla/js/src/Makefile.ref" ]]; then

            # use the old-style Makefile.ref build environment for spidermonkey

            if [[ $buildtype == "debug" ]]; then
                export JSBUILDOPT=
            else
                export JSBUILDOPT=BUILD_OPT=1
            fi

            if ! $buildbash $bashlogin -c "cd $BUILDTREE/mozilla/js/src; make -f Makefile.ref ${JSBUILDOPT} clean" 2>&1; then
                error "during js/src clean" $LINENO
            fi 

            if ! $buildbash $bashlogin -c "cd $BUILDTREE/mozilla/js/src; make -f Makefile.ref ${JSBUILDOPT}" 2>&1; then
                error "during js/src build" $LINENO
            fi

        else

            error "Neither Makefile.ref or autoconf builds available"

        fi
        ;;
esac


