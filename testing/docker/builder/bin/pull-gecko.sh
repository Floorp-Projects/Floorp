#! /bin/bash -e

target=$1

test $GECKO_BASE_REPOSITORY # base repository
test $GECKO_HEAD_REPOSITORY # repository to pull from
test $GECKO_HEAD_REF # reference if needed (usually same as rev)
test $GECKO_HEAD_REV # revision to checkout after pull

if [ ! -d $target ];
then
  echo 'Running initial clone of gecko...'
  tc-vcs clone $GECKO_BASE_REPOSITORY $target
fi

echo "Updating $target to $GECKO_HEAD_REPOSITORY $GECKO_HEAD_REF $GECKO_HEAD_REV"
tc-vcs checkout-revision \
  $target \
  $GECKO_HEAD_REPOSITORY \
  $GECKO_HEAD_REF \
  $GECKO_HEAD_REV
