#! /bin/bash -ve

while getopts "t:g:" arg; do
  case $arg in
    t)
      TAG=$OPTARG
      ;;
    g)
      GAIA_TESTVARS=$OPTARG
      ;;
  esac
done

pushd $(dirname $0)

test $TAG
test -f "$GAIA_TESTVARS"

mkdir -p data
cp $GAIA_TESTVARS data/gaia_testvars.json

docker build -t $TAG .
rm -f data/gaia_testvars.json
