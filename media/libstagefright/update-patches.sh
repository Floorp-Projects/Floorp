#!/bin/bash -e
cd `dirname "$0"`
rm -fR patches
for DIR in `find android/ -name .git`
do
    DIR=`dirname ${DIR}`
    DST=patches/${DIR:8}
    echo ${DST}
    mkdir -p `dirname ${DST}`
    cp -a ${DIR:8} `dirname ${DIR}`
    (cd ${DIR} && git diff) > ${DST}.patch
    (cd ${DIR} && git rev-parse HEAD) > ${DST}.tag
done
