#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# This tool contains functions that are to be used to handle/enable funsize
# Author: Mihai Tabara
#

HOOK=
AWS_BUCKET_NAME=
LOCAL_CACHE_DIR=

# Don't cache files smaller than this, as it's slower with S3
# Bug 1437473
CACHE_THRESHOLD=500000


NAMESPACE='releng.releases.partials'
if [ -e "${HOME}/.dogrc" ]
then
    METRIC_CMD="$(which dog)"
else
    METRIC_CMD="echo"
fi
METRIC_PARAMS="--type gauge --no_host"

if [ -n "${BRANCH}" ]
then
    if [ -n "${TAGS}" ]; then TAGS="${TAGS},"; fi
    TAGS="${TAGS}branch:${BRANCH}"
fi
if [ -n "${PLATFORM}" ]
then
    if [ -n "${TAGS}" ]; then TAGS="${TAGS},"; fi
    TAGS="${TAGS}platform:${PLATFORM}"
fi
if [ -n "${TAGS}" ]
then
    # We want literal quotes
    # shellcheck disable=SC2089
    TAGS="--tags='${TAGS}'"
fi

getsha512(){
    openssl sha512 "${1}" | awk '{print $2}'
}

print_usage(){
    echo "$(basename "$0") [-S S3-BUCKET-NAME] [-c LOCAL-CACHE-DIR-PATH] [-g] [-u] PATH-FROM-URL PATH-TO-URL PATH-PATCH"
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
    if [ "$(stat -c "%s" "$2")" -lt ${CACHE_THRESHOLD} ]
    then
      return 0
    fi
    sha_from=$(getsha512 "$1")
    sha_to=$(getsha512 "$2")
    patch_path="$3"
    patch_filename="$(basename "$3")"

    # save to local cache first
    if [ -n "$LOCAL_CACHE_DIR" ]; then
        local_cmd="mkdir -p "$LOCAL_CACHE_DIR/$sha_from""
        if $local_cmd >&2; then
            cp -avf "${patch_path}" "$LOCAL_CACHE_DIR/$sha_from/$sha_to"
            echo "${patch_path} saved on local cache."
        fi
    fi

    if [ -n "${AWS_BUCKET_NAME}" ]; then
        BUCKET_PATH="s3://${AWS_BUCKET_NAME}${sha_from}/${sha_to}/${patch_filename}"
        if aws s3 cp "${patch_path}" "${BUCKET_PATH}"; then
            echo "${patch_path} saved on s://${AWS_BUCKET_NAME}"
            return 0
        fi
        echo "${patch_path} failed to be uploaded to s3://${AWS_BUCKET_NAME}"
        return 1
    fi
    return 0
}

get_patch(){
    # $1 and $2 are the /path/to/filename
    if [ "$(stat -c "%s" "$2")" -lt ${CACHE_THRESHOLD} ]
    then
      return 1
    fi
    sha_from=$(getsha512 "$1")
    sha_to=$(getsha512 "$2")
    destination_file="$3"
    s3_filename="$(basename "$3")"

    # Try to retrieve from local cache first.
    if [ -n "$LOCAL_CACHE_DIR" ]; then
        if [ -r "$LOCAL_CACHE_DIR/$sha_from/$sha_to" ]; then
            cp -avf "$LOCAL_CACHE_DIR/$sha_from/$sha_to" "$destination_file"
            echo "Successful retrieved ${destination_file} from local cache."
            return 0
        fi
    fi
    # If not in the local cache, we might find it remotely.

    if [ -n "${AWS_BUCKET_NAME}" ]; then
        BUCKET_PATH="s3://${AWS_BUCKET_NAME}${sha_from}/${sha_to}/${s3_filename}"
        if aws s3 ls "${BUCKET_PATH}"; then
            # shellcheck disable=SC2086,SC2090
            ${METRIC_CMD} metric post "${NAMESPACE}.s3_cache.hit" "${S3_CACHE_HITS}" ${METRIC_PARAMS} ${TAGS}
            echo "s3 cache hits now ${S3_CACHE_HITS}"
            if aws s3 cp "${BUCKET_PATH}" "${destination_file}"; then
                echo "Successful retrieved ${destination_file} from s3://${AWS_BUCKET_NAME}"
                return 0
            else
                echo "Failed to retrieve ${destination_file} from s3://${AWS_BUCKET_NAME}"
                return 1
            fi
        # Not found, fall through to default error
        else
            # shellcheck disable=SC2086,SC2090
            ${METRIC_CMD} metric post "${NAMESPACE}.s3_cache.miss" "${S3_CACHE_MISSES}" ${METRIC_PARAMS} ${TAGS}
        fi
    fi
    return 1
}

OPTIND=1

while getopts ":S:c:gu" option; do
    case $option in
        S)
            # This will probably be bucketname/path/prefix but we can use it either way
            AWS_BUCKET_NAME="$OPTARG"
            # Ensure trailing slash is there.
            if [[ ! $AWS_BUCKET_NAME =~ .*/$ ]]; then
              AWS_BUCKET_NAME="${AWS_BUCKET_NAME}/"
            fi
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
