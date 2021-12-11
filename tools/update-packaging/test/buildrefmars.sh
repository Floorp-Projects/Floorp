#!/bin/bash
# Builds all the reference mars

if [ -f "ref.mar" ]; then
  rm "ref.mar"
fi
if [ -f "ref-mac.mar" ]; then
  rm "ref-mac.mar"
fi

 ../make_incremental_update.sh ref.mar `pwd`/from `pwd`/to
 ../make_incremental_update.sh ref-mac.mar `pwd`/from-mac `pwd`/to-mac

if [ -f "product-1.0.lang.platform.complete.mar" ]; then
  rm "product-1.0.lang.platform.complete.mar"
fi
if [ -f "product-2.0.lang.platform.complete.mar" ]; then
  rm "product-2.0.lang.platform.complete.mar"
fi
if [ -f "product-2.0.lang.mac.complete.mar" ]; then
  rm "product-2.0.lang.mac.complete.mar"
fi

./make_full_update.sh product-1.0.lang.platform.complete.mar "`pwd`/from"
./make_full_update.sh product-2.0.lang.platform.complete.mar "`pwd`/to"
./make_full_update.sh product-1.0.lang.mac.complete.mar "`pwd`/from-mac"
./make_full_update.sh product-2.0.lang.mac.complete.mar "`pwd`/to-mac"
