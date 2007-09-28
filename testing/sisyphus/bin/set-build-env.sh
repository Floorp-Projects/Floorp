#!/usr/local/bin/bash
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

export BUILDDIR=/work/mozilla/builds
export SHELL=/usr/local/bin/bash
export CONFIG_SHELL=/usr/local/bin/bash
export CONFIGURE_ENV_ARGS=/usr/local/bin/bash 
export MOZ_CVS_FLAGS="-z3 -q"
export CVSROOT=:pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot
export MOZILLA_OFFICIAL=1
export BUILD_OFFICIAL=1

#
# options processing
#
options="p:b:T:e:"
usage()
{
    cat <<EOF

usage: set-build-env.sh -p product -b branch -T buildtype [-e extra]

-p product      [firefox|thunderbird]
-b branch       [1.8.0|1.8.1|1.9.0]
-T buildtype    [opt|debug]
-e extra        extra qualifier to pick mozconfig and tree

EOF
}

myexit()
{
    myexit_status=$1

    case $0 in
        *bash*)
	        # prevent "sourced" script calls from 
            # exiting the current shell.
            break 99;;
        *)
            exit $myexit_status;;
    esac
}

for step in step1; do # dummy loop for handling exits

    unset product branch buildtype extra

    while getopts $options optname ; 
      do 
      case $optname in
          p) product=$OPTARG;;
          b) branch=$OPTARG;;
          T) buildtype=$OPTARG;;
          e) extra=$OPTARG;;
      esac
    done

    # echo product=$product, branch=$branch, buildtype=$buildtype, extra=$extra

    if [[ -z "$product" || -z "$branch" || -z "$buildtype" ]]; then
        echo -n "missing"
        if [[ -z "$product" ]]; then
            echo -n " -p product"
        fi
        if [[ -z "$branch" ]]; then
            echo -n " -b branch"
        fi
        if [[ -z "$buildtype" ]]; then
            echo -n " -T buildtype"
        fi
        usage
        myexit 1
    fi

    if [[ $branch == "1.8.0" ]]; then
        export BRANCH_CO_FLAGS="-r MOZILLA_1_8_0_BRANCH"
    elif [[ $branch == "1.8.1" ]]; then
        export BRANCH_CO_FLAGS="-r MOZILLA_1_8_BRANCH"
    elif [[ $branch == "1.9.0" ]]; then
        export BRANCH_CO_FLAGS="";
    else
        echo "Unknown branch: $branch"
        myexit 1
    fi

    if [[ -n "$MOZ_CO_DATE" ]]; then
        export DATE_CO_FLAGS="-D $MOZ_CO_DATE"
    fi

    if [[ -n "$WINDIR" ]] ; then
        OSID=win32
        export platform=i686

        if echo $branch | egrep -q '^1\.8'; then
            export MOZ_TOOLS="/work/mozilla/moztools"
            source /work/mozilla/mozilla.com/test.mozilla.com/www/bin/set-msvc6-env.sh
        else
            export MOZ_TOOLS="/work/mozilla/moztools-static"
            source /work/mozilla/mozilla.com/test.mozilla.com/www/bin/set-msvc8-env.sh
        fi

        echo moztools Location: $MOZ_TOOLS

    elif uname | grep -iq darwin ; then
        OSID=mac
        export platform=`uname -p`
    else
        OSID=linux
        export platform=i686
    fi

    if [[ -z $extra ]]; then
        export TREE="$BUILDDIR/$branch"
    else
        export TREE="$BUILDDIR/$branch-$extra"

        #
        # extras can't be placed in mozconfigs since not all parts
        # of the build system use mozconfig (e.g. js shell) and since
        # the obj directory is not configurable for them as well thus
        # requiring separate source trees
        #

        case "$extra" in
            too-much-gc)
            export XCFLAGS="-DWAY_TOO_MUCH_GC=1"
            export CFLAGS="-DWAY_TOO_MUCH_GC=1"
            export CXXFLAGS="-DWAY_TOO_MUCH_GC=1"
            ;;
            gcov)

            if [[ "$OSID" == "win32" ]]; then
                echo "win32 does not support gcov"
                    myexit 1
            fi
            export CFLAGS="--coverage"
            export CXXFLAGS="--coverage"
            export XCFLAGS="--coverage"
            export OS_CFLAGS="--coverage"
            export LDFLAGS="--coverage"
            export XLDOPTS="--coverage"	
            ;;
            jprof)
            ;;
        esac
    fi

    if [[ ! -d $TREE ]]; then
        echo "Build directory $TREE does not exist"
        myexit 2
    fi

    # here project refers to either browser or mail
    # and is used to find mozilla/(browser|mail)/config/mozconfig
    if [[ $product == "firefox" ]]; then
        project=browser
        export MOZCONFIG="$TREE/mozconfig-firefox-$OSID-$platform-$buildtype"
    elif [[ $product == "thunderbird" ]]; then
        project=mail
        export MOZCONFIG="$TREE/mozconfig-thunderbird-$OSID-$platform-$buildtype"
    else
        echo "Assuming project=browser for product: $product"
        project=browser
        export MOZCONFIG="$TREE/mozconfig-firefox-$OSID-$platform-$buildtype"
    fi

    # js shell builds
    if [[ $buildtype == "debug" ]]; then
        unset BUILD_OPT
    else
        export BUILD_OPT=1
    fi

    case "$OSID" in
        mac)
        export JS_EDITLINE=1 # required for mac
        ;;
    esac
    # end js shell builds

    set | sed 's/^/environment: /'
    echo "mozconfig: $MOZCONFIG"
    cat $MOZCONFIG | sed 's/^/mozconfig: /'
done

