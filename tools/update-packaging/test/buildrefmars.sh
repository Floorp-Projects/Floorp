#!/bin/bash
# Builds all the reference mars

rm ref.mar
rm ref-mac.mar

 ../make_incremental_update.sh ref.mar `pwd`/from `pwd`/to
 ../make_incremental_update.sh ref-mac.mar `pwd`/from `pwd`/to-mac

rm product-1.0.lang.platform.complete.mar
rm product-2.0.lang.platform.complete.mar
rm product-2.0.lang.mac.complete.mar

./make_full_update.sh product-1.0.lang.platform.complete.mar `pwd`/from
./make_full_update.sh product-2.0.lang.platform.complete.mar `pwd`/to 
./make_full_update.sh product-2.0.lang.mac.complete.mar `pwd`/to-mac 
