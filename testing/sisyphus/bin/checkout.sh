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

if [[ -z "$BUILDTREE" ]]; then
    error "source tree not specified!" $LINENO
fi

case $branch in
    1.8.0);;
    1.8.1);;
    1.9.0);;
    *)
        if [[ -z "$TEST_MOZILLA_HG" ]]; then
            error "environment variable TEST_MOZILLA_HG must be set to the hg repository for branch $branch"
        fi
        ;;
esac

if [[ -n "$TEST_MOZILLA_HG" ]]; then
    # maintain a local copy of the hg repository
    # clone specific trees from it.

    TEST_MOZILLA_HG_LOCAL=${TEST_MOZILLA_HG_LOCAL:-$BUILDDIR/hg.mozilla.org/`basename $TEST_MOZILLA_HG`}

    if [[ ! -d $BUILDDIR/hg.mozilla.org ]]; then
        mkdir $BUILDDIR/hg.mozilla.org
    fi

    if [[ ! -d $TEST_MOZILLA_HG_LOCAL ]]; then
        if ! hg clone $TEST_MOZILLA_HG $TEST_MOZILLA_HG_LOCAL; then
            error "during hg clone of $TEST_MOZILLA_HG" $LINENO
        fi
    fi

    cd $TEST_MOZILLA_HG_LOCAL
    hg pull
    if [[ "$OSID" == "nt" ]]; then
        # remove spurious lock file
        rm -f $TEST_MOZILLA_HG_LOCAL/.hg/wlock.lnk
    fi
    hg update -C
    echo "`hg root` id `hg id`"
fi

cd $BUILDTREE

if [[ -n "$TEST_MOZILLA_HG" ]]; then

    if [[ ! -d mozilla/.hg ]]; then
        if ! hg clone $TEST_MOZILLA_HG_LOCAL $BUILDTREE/mozilla; then
            error "during hg clone of $TEST_MOZILLA_HG_LOCAL" $LINENO
        fi
    fi

    cd mozilla
    hg pull
    if [[ "$OSID" == "nt" ]]; then
        # remove spurious lock file
        rm -f $TEST_MOZILLA_HG/.hg/wlock.lnk
    fi
    hg update -C

    hg update -r $TEST_MOZILLA_HG_REV

    echo "`hg root` id `hg id`"

    if [[ -n "$DATE_CO_FLAGS" ]]; then
        eval hg update $DATE_CO_FLAGS
        echo "Update to date $MOZ_CO_DATE `hg root` id `hg id`"
    fi
    
fi

case $product in
    firefox)
        case $branch in
            1.8.*|1.9.0)
                if [[ ! ( -d mozilla && \
                    -e mozilla/client.mk && \
                    -e "mozilla/$project/config/mozconfig" ) ]]; then
                    if ! eval cvs -z3 -q co $MOZ_CO_FLAGS $BRANCH_CO_FLAGS $DATE_CO_FLAGS \
                        mozilla/client.mk mozilla/$project/config/mozconfig; then
                        error "during checkout of $project mozconfig" $LINENO
                    fi
                fi
                if ! $buildbash $bashlogin -c "export PATH=\"$BUILDPATH\"; cd $BUILDTREE/mozilla; make -f client.mk checkout" 2>&1; then
                    error "during checkout of $project tree" $LINENO
                fi
                ;;

            1.9.1|1.9.2)

                # do not use mozilla-build on windows systems as we 
                # must use the cygwin python with the cygwin mercurial.

                if ! python client.py checkout; then
                    error "during checkout of $project tree" $LINENO
                fi
                ;;

            *)
                error "branch $branch not yet supported"
                ;;
        esac
        ;;

    thunderbird)

        case $branch in
            1.8.*|1.9.0)
                if [[ ! ( -d mozilla && \
                    -e mozilla/client.mk && \
                    -e "mozilla/$project/config/mozconfig" ) ]]; then
                    if ! eval cvs -z3 -q co $MOZ_CO_FLAGS $BRANCH_CO_FLAGS $DATE_CO_FLAGS \
                        mozilla/client.mk mozilla/$project/config/mozconfig; then
                        error "during checkout of $MOZ_CO_FLAGS $BRANCH_CO_FLAGS $DATE_CO_FLAGS $project mozconfig" $LINENO
                    fi
                fi

                if [[ ! ( -d mozilla && \
                    -e mozilla/client.mk && \
                    -e "mozilla/browser/config/mozconfig" ) ]]; then
                    if ! eval cvs -z3 -q co $MOZ_CO_FLAGS $BRANCH_CO_FLAGS $DATE_CO_FLAGS \
                        mozilla/client.mk mozilla/browser/config/mozconfig; then
                        error "during checkout of $MOZ_CO_FLAGS $BRANCH_CO_FLAGS $DATE_CO_FLAGS browser mozconfig" $LINENO
                    fi
                fi

                if ! $buildbash $bashlogin -c "export PATH=\"$BUILDPATH\"; cd $BUILDTREE/mozilla; make -f client.mk checkout" 2>&1; then
                    error "during checkout of $project tree" $LINENO
                fi
                ;;

            1.9.1|1.9.2)

                # do not use mozilla-build on windows systems as we 
                # must use the cygwin python with the cygwin mercurial.

                if ! python client.py checkout; then
                    error "during checkout of $project tree" $LINENO
                fi
                ;;

            *)
                error "branch $branch not yet supported"
                ;;
        esac
        ;;

    fennec)

        case $branch in
            1.9.1|1.9.2)

                # XXX need to generalize the mobile-browser repository
                if [[ ! -d mobile/.hg ]]; then
                    if ! hg clone http://hg.mozilla.org/mobile-browser $BUILDTREE/mozilla/mobile; then
                        error "during hg clone of http://hg.mozilla.org/mobile-browser" $LINENO
                    fi
                fi

                cd mobile
                hg pull
                if [[ "$OSID" == "nt" ]]; then
                    # remove spurious lock file
                    rm -f .hg/wlock.lnk
                fi
                hg update -C

                # XXX need to deal with mobile revisions from different repositories

                cd ../

                # do not use mozilla-build on windows systems as we
                # must use the cygwin python with the cygwin mercurial.

                if ! python client.py checkout; then
                    error "during checkout of $project tree" $LINENO
                fi
                ;;

            *)
                error "branch $branch not yet supported"
                ;;
        esac
        ;;

    js) 

        case $branch in
            1.8.*|1.9.0)
                if [[ ! ( -d mozilla && \
                    -e mozilla/js && \
                    -e mozilla/js/src ) ]]; then
                    if ! eval cvs -z3 -q co $MOZ_CO_FLAGS $BRANCH_CO_FLAGS $DATE_CO_FLAGS mozilla/js; then
                        error "during initial co $MOZ_CO_FLAGS $BRANCH_CO_FLAGS $DATE_CO_FLAGS mozilla/js"
                    fi
                fi

                cd mozilla/js/src

                if ! eval cvs -z3 -q update $MOZ_CO_FLAGS $BRANCH_CO_FLAGS $DATE_CO_FLAGS -d -P 2>&1; then
                    error "during update $MOZ_CO_FLAGS $BRANCH_CO_FLAGS $DATE_CO_FLAGS js/src" $LINENO
                fi

                if ! cvs -z3 -q update -d -P -A editline config  2>&1; then
                    error "during checkout of js/src" $LINENO
                fi
                ;;

            1.9.1|1.9.2)

                # do not use mozilla-build on windows systems as we 
                # must use the cygwin python with the cygwin mercurial.

                if ! python client.py checkout; then
                    error "during checkout of $project tree" $LINENO
                fi

                cd js/src
                ;;

            *)
                error "branch $branch not yet supported"
                ;;
        esac
        # end for js shell
        ;;
    *)
        error "unknown product $product" $LINENO
        ;;
esac
