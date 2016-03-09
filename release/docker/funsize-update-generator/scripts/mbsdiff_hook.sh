#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# This tool contains functions that are to be used to handle/enable funsize
# Author: Mihai Tabara
#

HOOK=
SERVER_URL=
LOCAL_CACHE_DIR=

getsha512(){
    echo "$(openssl sha512 "${1}" | awk '{print $2}')"
}

print_usage(){
    echo "$(basename $0) -A SERVER-URL [-c LOCAL-CACHE-DIR-PATH] [-g] [-u] PATH-FROM-URL PATH-TO-URL PATH-PATCH"
    echo "Script that saves/retrieves from cache presumptive patches as args"
    echo ""
    echo "-A SERVER-URL - host where to send the files"
    echo "-c LOCAL-CACHE-DIR-PATH local path to which patches are cached"
    echo "-g pre hook - tests whether patch already in cache"
    echo "-u post hook - upload patch to cache for future use"
    echo ""
    echo "PATH-FROM-URL     : path on disk for source file"
    echo "PATH-TO-URL       : path on disk for destination file"
    echo "PATH-PATCH        : path on disk for patch between source and destination"
}

upload_patch(){
    sha_from=`getsha512 "$1"`
    sha_to=`getsha512 "$2"`
    patch_path="$3"

    # save to local cache first
    if [ -n "$LOCAL_CACHE_DIR" ]; then
        local_cmd="mkdir -p "$LOCAL_CACHE_DIR/$sha_from""
        if `$local_cmd` >&2; then
            cp -avf "$patch_path" "$LOCAL_CACHE_DIR/$sha_from/$sha_to"
            echo "$patch_path saved on local cache!"
        fi
    fi
    # The remote cache implementation is not used. The code is for usage
    # reference only.
     return 0

    # send it over to funsize
    cmd="curl -sSw %{http_code} -o /dev/null -X POST $SERVER_URL -F sha_from="$sha_from" -F sha_to="$sha_to" -F patch_file="@$patch_path""
    ret_code=`$cmd`

    if [ $ret_code -eq 200 ]; then
        echo "$patch_path Successful uploaded to funsize!"
        return 0
    fi

    echo "$patch_path Failed to be uploaded to funsize!"
    return 1
}

get_patch(){
    sha_from=`getsha512 "$1"`
    sha_to=`getsha512 "$2"`
    destination_file="$3"
    tmp_file="$destination_file.tmp"

    # try to retrieve from local cache first
    if [ -r "$LOCAL_CACHE_DIR/$sha_from/$sha_to" ]; then
        cp -avf "$LOCAL_CACHE_DIR/$sha_from/$sha_to" "$destination_file"
        echo "Successful retrieved $destination_file from local cache!"
        return 0
    else
        echo "File is not in the locale cache"
        return 1
    fi
    # The remote cache implementation is not used. The code is for usage
    # reference only.

    # if unsuccessful, try to retrieve from funsize
    cmd="curl -LsSGw %{http_code} $SERVER_URL/$sha_from/$sha_to -o $tmp_file"
    ret_code=`$cmd`

    if [ $ret_code -eq 200 ]; then
        mv "$tmp_file" "$destination_file"
        echo "Successful retrieved $destination_file from funsize!"
        return 0
    fi

    rm  -f "$tmp_file"
    echo "Failed to retrieve $destination_file from funsize!"
    return 1
}

OPTIND=1

while getopts ":A:c:gu" option; do
    case $option in
        A)
            SERVER_URL="$OPTARG"
            ;;
        c)
            LOCAL_CACHE_DIR="$OPTARG"
            ;;
        g)
            HOOK="PRE"
            ;;
        u)
            HOOK="POST"
            ;;
        \?)
            echo "Invalid option: -$OPTARG" >&2
            print_usage
            exit 1
            ;;
        :)
            echo "Option -$OPTARG requires an argument." >&2
            print_usage
            exit 1
            ;;
        *)
            echo "Unimplemented option: -$OPTARG" >&2
            print_usage
            exit 1
            ;;
    esac
done
shift $((OPTIND-1))

if [ "$HOOK" == "PRE" ]; then
    get_patch "$1" "$2" "$3"
elif [ "$HOOK" == "POST" ]; then
    upload_patch "$1" "$2" "$3"
fi
