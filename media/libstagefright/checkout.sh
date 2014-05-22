#!/bin/bash -e
set -o pipefail
abort () {
    errcode=$?
    echo "Error: That didn't work..."
    exit $errcode
}
trap abort ERR

cd `dirname "$0"`

SITE=https://android.googlesource.com/platform
for TAGFILE in `find patches -name \*.tag` 
do
    REPO=${TAGFILE:8:-4}
    DEST=android/${REPO}
    if [[ ! -e ${DEST} ]]
    then
        mkdir -p `dirname ${DEST}`
        echo Cloning from ${SITE}/${REPO}
        git clone ${SITE}/${REPO} ${DEST}
    fi

    rm -fR ${REPO}
    TAG=`cat $TAGFILE`
    (cd $DEST && git reset --hard 2>&1 && git checkout ${TAG} 2>&1) > /dev/null
done

FILES=`python files.py`
HEADERS=`cat additional_headers`
for FILE in $FILES $HEADERS frameworks/av/media/libstagefright/include/AMRExtractor.h
do
    echo Copying ${FILE}
    mkdir -p `dirname ${FILE}`
    cp android/${FILE} ${FILE}
done

for PATCH in `find patches -name \*.patch` 
do
    REPO=${PATCH:8:-6}
    echo Patching repo ${REPO}
    for FILE in `grep -- '--- a/' ${PATCH} | colrm 1 6`
    do
        if [[ ! -e ${FILE} ]]
        then
            echo Copying ${REPO}/${FILE}
            mkdir -p `dirname ${REPO}/${FILE}`
            cp android/${REPO}/${FILE} ${REPO}/${FILE}
        fi
    done
    (cd ${REPO} && patch -p1 || true) < $PATCH
done
