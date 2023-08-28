#!/usr/bin/env sh

set -x

export success=true

find . -iname '*.rs' | while read -r file; do
    result=$(grep '.\{79\}' "${file}" | grep --invert 'http')
    if [ "${result}" = "" ]
    then
        :
    else
        echo "file \"${file}\": $result"
        exit 1
    fi
done

