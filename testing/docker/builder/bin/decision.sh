#! /bin/bash -ex

DEST=/home/worker/source/gecko

test $GECKO_BASE_REPOSITORY # e.g. https://hg.mozilla.org/mozilla-central
test $GECKO_HEAD_REPOSITORY # e.g. https://hg.mozilla.org/try
test $GECKO_HEAD_REF # <cset>
test $GECKO_HEAD_REV # <cset>

tc-vcs checkout $DEST \
  $GECKO_BASE_REPOSITORY \
  $GECKO_HEAD_REPOSITORY \
  $GECKO_HEAD_REV \
  $GECKO_HEAD_REF

echo "At revision: $(tc-vcs revision $DEST)"
cd $DEST
