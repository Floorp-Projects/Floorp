#! /bin/bash -e

target=$1

test $GECKO_BASE_REPOSITORY # base repository
test $GECKO_HEAD_REPOSITORY # repository to pull from
test $GECKO_HEAD_REF # reference if needed (usually same as rev)
test $GECKO_HEAD_REV # revision to checkout after pull

tc-vcs checkout $target \
  $GECKO_BASE_REPOSITORY \
  $GECKO_HEAD_REPOSITORY \
  $GECKO_HEAD_REV \
  $GECKO_HEAD_REF \
