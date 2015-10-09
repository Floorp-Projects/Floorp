#!/bin/bash -vex
test $GAIA_BASE_REPOSITORY # ex: https://github.com/mozilla-b2g/gaia
test $GAIA_HEAD_REPOSITORY # ex: https://github.com/mozilla-b2g/gaia
test $GAIA_REF # ex: master
test $GAIA_REV # ex: master

tc-vcs \
  checkout \
  /home/worker/gaia \
  $GAIA_BASE_REPOSITORY \
  $GAIA_HEAD_REPOSITORY \
  $GAIA_REV \
  $GAIA_REF \
