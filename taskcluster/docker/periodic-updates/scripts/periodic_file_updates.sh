#!/bin/bash

set -ex

function usage {
  cat <<EOF

Usage: $(basename "$0") -h # Displays this usage/help text
Usage: $(basename "$0") -x # lists exit codes
Usage: $(basename "$0") [-p product]
           [-r existing_repo_dir]
           # Use mozilla-central builds to check HSTS & HPKP
           [--use-mozilla-central]
           # Use archive.m.o instead of the taskcluster index to get xpcshell
           [--use-ftp-builds]
           # One (or more) of the following actions must be specified.
           --hsts | --hpkp | --remote-settings | --suffix-list | --mobile-experiments
           -b branch

EOF
}

PRODUCT="firefox"
BRANCH=""
PLATFORM_EXT="tar.bz2"
UNPACK_CMD="tar jxf"
CLOSED_TREE=false
DONTBUILD=false
APPROVAL=false
COMMIT_AUTHOR='ffxbld <ffxbld@mozilla.com>'
REPODIR=''
HGHOST="hg.mozilla.org"
STAGEHOST="archive.mozilla.org"
WGET="wget -nv"
UNTAR="tar -zxf"
DIFF="$(command -v diff) -u"
BASEDIR="${HOME}"

SCRIPTDIR="$(realpath "$(dirname "$0")")"
HG="$(command -v hg)"
DATADIR="${BASEDIR}/data"
mkdir -p "${DATADIR}"

USE_MC=false
USE_TC=true
JQ="$(command -v jq)"

DO_HSTS=false
HSTS_PRELOAD_SCRIPT="${SCRIPTDIR}/getHSTSPreloadList.js"
HSTS_PRELOAD_ERRORS="nsSTSPreloadList.errors"
HSTS_PRELOAD_INC_OLD="${DATADIR}/nsSTSPreloadList.inc"
HSTS_PRELOAD_INC_NEW="${BASEDIR}/${PRODUCT}/nsSTSPreloadList.inc"
HSTS_UPDATED=false

DO_HPKP=false
HPKP_PRELOAD_SCRIPT="${SCRIPTDIR}/genHPKPStaticPins.js"
HPKP_PRELOAD_ERRORS="StaticHPKPins.errors"
HPKP_PRELOAD_JSON="${DATADIR}/PreloadedHPKPins.json"
HPKP_PRELOAD_INC="StaticHPKPins.h"
HPKP_PRELOAD_INPUT="${DATADIR}/${HPKP_PRELOAD_INC}"
HPKP_PRELOAD_OUTPUT="${DATADIR}/${HPKP_PRELOAD_INC}.out"
HPKP_UPDATED=false

DO_REMOTE_SETTINGS=false
REMOTE_SETTINGS_SERVER=''
REMOTE_SETTINGS_INPUT="${DATADIR}/remote-settings.in"
REMOTE_SETTINGS_OUTPUT="${DATADIR}/remote-settings.out"
REMOTE_SETTINGS_DIR="/services/settings/dumps"
REMOTE_SETTINGS_UPDATED=false

DO_SUFFIX_LIST=false
GITHUB_SUFFIX_URL="https://raw.githubusercontent.com/publicsuffix/list/master/public_suffix_list.dat"
GITHUB_SUFFIX_LOCAL="public_suffix_list.dat"
HG_SUFFIX_LOCAL="effective_tld_names.dat"
HG_SUFFIX_PATH="/netwerk/dns/${HG_SUFFIX_LOCAL}"
SUFFIX_LIST_UPDATED=false

DO_MOBILE_EXPERIMENTS=false
EXPERIMENTER_URL="https://experimenter.services.mozilla.com/api/v6/experiments-first-run/"
FENIX_INITIAL_EXPERIMENTS="mobile/android/fenix/app/src/main/res/raw/initial_experiments.json"
FOCUS_INITIAL_EXPERIMENTS="mobile/android/focus-android/app/src/main/res/raw/initial_experiments.json"
MOBILE_EXPERIMENTS_UPDATED=false

ARTIFACTS_DIR="${ARTIFACTS_DIR:-.}"
# Defaults
HSTS_DIFF_ARTIFACT="${ARTIFACTS_DIR}/${HSTS_DIFF_ARTIFACT:-"nsSTSPreloadList.diff"}"
HPKP_DIFF_ARTIFACT="${ARTIFACTS_DIR}/${HPKP_DIFF_ARTIFACT:-"StaticHPKPins.h.diff"}"
REMOTE_SETTINGS_DIFF_ARTIFACT="${ARTIFACTS_DIR}/${REMOTE_SETTINGS_DIFF_ARTIFACT:-"remote-settings.diff"}"
SUFFIX_LIST_DIFF_ARTIFACT="${ARTIFACTS_DIR}/${SUFFIX_LIST_DIFF_ARTIFACT:-"effective_tld_names.diff"}"
EXPERIMENTER_DIFF_ARTIFACT="${ARTIFACTS_DIR}/initial_experiments.diff"

# duplicate the functionality of taskcluster-lib-urls, but in bash..
queue_base="$TASKCLUSTER_ROOT_URL/api/queue/v1"
index_base="$TASKCLUSTER_ROOT_URL/api/index/v1"

# Cleanup common artifacts.
function preflight_cleanup {
  cd "${BASEDIR}"
  rm -rf "${PRODUCT}" tests "${BROWSER_ARCHIVE}" "${TESTS_ARCHIVE}"
}

function download_shared_artifacts_from_ftp {
  cd "${BASEDIR}"

  # Download everything we need to run js with xpcshell
  echo "INFO: Downloading all the necessary pieces from ${STAGEHOST}..."
  ARTIFACT_DIR="nightly/latest-${REPODIR}"
  if [ "${USE_MC}" == "true" ]; then
    ARTIFACT_DIR="nightly/latest-mozilla-central"
  fi

  BROWSER_ARCHIVE_URL="https://${STAGEHOST}/pub/mozilla.org/${PRODUCT}/${ARTIFACT_DIR}/${BROWSER_ARCHIVE}"
  TESTS_ARCHIVE_URL="https://${STAGEHOST}/pub/mozilla.org/${PRODUCT}/${ARTIFACT_DIR}/${TESTS_ARCHIVE}"

  echo "INFO: ${WGET} ${BROWSER_ARCHIVE_URL}"
  ${WGET} "${BROWSER_ARCHIVE_URL}"
  echo "INFO: ${WGET} ${TESTS_ARCHIVE_URL}"
  ${WGET} "${TESTS_ARCHIVE_URL}"
}

function download_shared_artifacts_from_tc {
  cd "${BASEDIR}"
  TASKID_FILE="taskId.json"

  # Download everything we need to run js with xpcshell
  echo "INFO: Downloading all the necessary pieces from the taskcluster index..."
  TASKID_URL="$index_base/task/gecko.v2.${REPODIR}.shippable.latest.${PRODUCT}.linux64-opt"
  if [ "${USE_MC}" == "true" ]; then
    TASKID_URL="$index_base/task/gecko.v2.mozilla-central.shippable.latest.${PRODUCT}.linux64-opt"
  fi
  ${WGET} -O ${TASKID_FILE} "${TASKID_URL}"
  INDEX_TASK_ID="$($JQ -r '.taskId' ${TASKID_FILE})"
  if [ -z "${INDEX_TASK_ID}" ]; then
    echo "Failed to look up taskId at ${TASKID_URL}"
    exit 22
  else
    echo "INFO: Got taskId of $INDEX_TASK_ID"
  fi

  TASKSTATUS_FILE="taskstatus.json"
  STATUS_URL="$queue_base/task/${INDEX_TASK_ID}/status"
  ${WGET} -O "${TASKSTATUS_FILE}" "${STATUS_URL}"
  LAST_RUN_INDEX=$(($(jq '.status.runs | length' ${TASKSTATUS_FILE}) - 1))
  echo "INFO: Examining run number ${LAST_RUN_INDEX}"

  BROWSER_ARCHIVE_URL="$queue_base/task/${INDEX_TASK_ID}/runs/${LAST_RUN_INDEX}/artifacts/public/build/${BROWSER_ARCHIVE}"
  echo "INFO: ${WGET} ${BROWSER_ARCHIVE_URL}"
  ${WGET} "${BROWSER_ARCHIVE_URL}"

  TESTS_ARCHIVE_URL="$queue_base/task/${INDEX_TASK_ID}/runs/${LAST_RUN_INDEX}/artifacts/public/build/${TESTS_ARCHIVE}"
  echo "INFO: ${WGET} ${TESTS_ARCHIVE_URL}"
  ${WGET} "${TESTS_ARCHIVE_URL}"
}

function unpack_artifacts {
  cd "${BASEDIR}"
  if [ ! -f "${BROWSER_ARCHIVE}" ]; then
    echo "Downloaded file '${BROWSER_ARCHIVE}' not found in directory '$(pwd)'." >&2
    exit 31
  fi
  if [ ! -f "${TESTS_ARCHIVE}" ]; then
    echo "Downloaded file '${TESTS_ARCHIVE}' not found in directory '$(pwd)'." >&2
    exit 32
  fi
  # Unpack the browser and move xpcshell in place for updating the preload list.
  echo "INFO: Unpacking resources..."
  ${UNPACK_CMD} "${BROWSER_ARCHIVE}"
  mkdir -p tests
  cd tests
  ${UNTAR} "../${TESTS_ARCHIVE}"
  cd "${BASEDIR}"
  cp tests/bin/xpcshell "${PRODUCT}"
}

# Downloads the current in-tree HSTS (HTTP Strict Transport Security) files.
# Runs a simple xpcshell script to generate up-to-date HSTS information.
# Compares the new HSTS output with the old to determine whether we need to update.
function compare_hsts_files {
  cd "${BASEDIR}"

  HSTS_PRELOAD_INC_HG="${HGREPO}/raw-file/default/security/manager/ssl/$(basename "${HSTS_PRELOAD_INC_OLD}")"

  echo "INFO: Downloading existing include file..."
  rm -rf "${HSTS_PRELOAD_ERRORS}" "${HSTS_PRELOAD_INC_OLD}"
  echo "INFO: ${WGET} ${HSTS_PRELOAD_INC_HG}"
  ${WGET} -O "${HSTS_PRELOAD_INC_OLD}" "${HSTS_PRELOAD_INC_HG}"

  if [ ! -f "${HSTS_PRELOAD_INC_OLD}" ]; then
    echo "Downloaded file '${HSTS_PRELOAD_INC_OLD}' not found in directory '$(pwd)' - this should have been downloaded above from ${HSTS_PRELOAD_INC_HG}." >&2
    exit 41
  fi

  # Run the script to get an updated preload list.
  echo "INFO: Generating new HSTS preload list..."
  cd "${BASEDIR}/${PRODUCT}"
  if ! LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:. ./xpcshell "${HSTS_PRELOAD_SCRIPT}" "${HSTS_PRELOAD_INC_OLD}"; then
    echo "HSTS preload list generation failed" >&2
    exit 43
  fi

  # The created files should be non-empty.
  echo "INFO: Checking whether new HSTS preload list is valid..."
  if [ ! -s "${HSTS_PRELOAD_INC_NEW}" ]; then
    echo "New HSTS preload list ${HSTS_PRELOAD_INC_NEW} is empty. That's less good." >&2
    exit 42
  fi
  cd "${BASEDIR}"

  # Check for differences
  echo "INFO: diffing old/new HSTS preload lists into ${HSTS_DIFF_ARTIFACT}"
  ${DIFF} "${HSTS_PRELOAD_INC_OLD}" "${HSTS_PRELOAD_INC_NEW}" | tee "${HSTS_DIFF_ARTIFACT}"
  if [ -s "${HSTS_DIFF_ARTIFACT}" ]
  then
    return 0
  fi
  return 1
}

# Downloads the current in-tree HPKP (HTTP public key pinning) files.
# Runs a simple xpcshell script to generate up-to-date HPKP information.
# Compares the new HPKP output with the old to determine whether we need to update.
function compare_hpkp_files {
  cd "${BASEDIR}"
  HPKP_PRELOAD_JSON_HG="${HGREPO}/raw-file/default/security/manager/tools/$(basename "${HPKP_PRELOAD_JSON}")"

  HPKP_PRELOAD_OUTPUT_HG="${HGREPO}/raw-file/default/security/manager/ssl/${HPKP_PRELOAD_INC}"

  rm -f "${HPKP_PRELOAD_OUTPUT}"
  ${WGET} -O "${HPKP_PRELOAD_INPUT}" "${HPKP_PRELOAD_OUTPUT_HG}"
  ${WGET} -O "${HPKP_PRELOAD_JSON}" "${HPKP_PRELOAD_JSON_HG}"

  # Run the script to get an updated preload list.
  echo "INFO: Generating new HPKP preload list..."
  cd "${BASEDIR}/${PRODUCT}"
  if ! LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:. ./xpcshell "${HPKP_PRELOAD_SCRIPT}" "${HPKP_PRELOAD_JSON}" "${HPKP_PRELOAD_OUTPUT}" > "${HPKP_PRELOAD_ERRORS}"; then
    echo "HPKP preload list generation failed" >&2
    exit 54
  fi

  # The created files should be non-empty.
  echo "INFO: Checking whether new HPKP preload list is valid..."

  if [ ! -s "${HPKP_PRELOAD_OUTPUT}" ]; then
    echo "${HPKP_PRELOAD_OUTPUT} is empty. That's less good." >&2
    exit 52
  fi
  if ! grep kPreloadPKPinsExpirationTime "${HPKP_PRELOAD_OUTPUT}"; then
    echo "${HPKP_PRELOAD_OUTPUT} is missing an expiration time. Truncated?" >&2
    exit 53
  fi
  cd "${BASEDIR}"

  echo "INFO: diffing old/new HPKP preload lists..."
  ${DIFF} "${HPKP_PRELOAD_INPUT}" "${HPKP_PRELOAD_OUTPUT}" | tee "${HPKP_DIFF_ARTIFACT}"
  if [ -s "${HPKP_DIFF_ARTIFACT}" ]
  then
    return 0
  fi
  return 1
}

function is_valid_xml {
  xmlfile=$1
  XMLLINT=$(command -v xmllint 2>/dev/null | head -n1)

  if [ ! -x "${XMLLINT}" ]; then
    echo "ERROR: xmllint not found in PATH"
    exit 60
  fi
  ${XMLLINT} --nonet --noout "${xmlfile}"
}

# Downloads the public suffix list
function compare_suffix_lists {
  HG_SUFFIX_URL="${HGREPO}/raw-file/default/${HG_SUFFIX_PATH}"
  cd "${BASEDIR}"

  echo "INFO: ${WGET} -O ${GITHUB_SUFFIX_LOCAL} ${GITHUB_SUFFIX_URL}"
  rm -f "${GITHUB_SUFFIX_LOCAL}"
  ${WGET} -O "${GITHUB_SUFFIX_LOCAL}" "${GITHUB_SUFFIX_URL}"

  echo "INFO: ${WGET} -O ${HG_SUFFIX_LOCAL} ${HG_SUFFIX_URL}"
  rm -f "${HG_SUFFIX_LOCAL}"
  ${WGET} -O "${HG_SUFFIX_LOCAL}" "${HG_SUFFIX_URL}"

  echo "INFO: diffing in-tree suffix list against the suffix list from AMO..."
  ${DIFF} ${GITHUB_SUFFIX_LOCAL} ${HG_SUFFIX_LOCAL} | tee "${SUFFIX_LIST_DIFF_ARTIFACT}"
  if [ -s "${SUFFIX_LIST_DIFF_ARTIFACT}" ]
  then
    return 0
  fi
  return 1
}

function compare_remote_settings_files {
  REMOTE_SETTINGS_SERVER="https://firefox.settings.services.mozilla.com/v1"

  # 1. List remote settings collections from server.
  echo "INFO: fetch remote settings list from server"
  ${WGET} -qO- "${REMOTE_SETTINGS_SERVER}/buckets/monitor/collections/changes/records" |\
    ${JQ} -r '.data[] | .bucket+"/"+.collection+"/"+(.last_modified|tostring)' |\
    # 2. For each entry ${bucket, collection, last_modified}
  while IFS="/" read -r bucket collection last_modified; do

    # 3. Download the dump from HG into REMOTE_SETTINGS_INPUT folder
    hg_dump_url="${HGREPO}/raw-file/default${REMOTE_SETTINGS_DIR}/${bucket}/${collection}.json"
    local_location_input="$REMOTE_SETTINGS_INPUT/${bucket}/${collection}.json"
    mkdir -p "$REMOTE_SETTINGS_INPUT/${bucket}"
    ${WGET} -qO "$local_location_input" "$hg_dump_url"
    if [ $? -eq 8 ]; then
      # We don't keep any dump for this collection, skip it.
      # Try to clean up in case no collection in this bucket has dump.
      rmdir "$REMOTE_SETTINGS_INPUT/${bucket}" --ignore-fail-on-non-empty
      continue
    fi

    # 4. Download server version into REMOTE_SETTINGS_OUTPUT folder
    remote_records_url="$REMOTE_SETTINGS_SERVER/buckets/${bucket}/collections/${collection}/changeset?_expected=${last_modified}"
    local_location_output="$REMOTE_SETTINGS_OUTPUT/${bucket}/${collection}.json"
    mkdir -p "$REMOTE_SETTINGS_OUTPUT/${bucket}"
    ${WGET} -qO- "$remote_records_url" | ${JQ} '{"data": .changes, "timestamp": .timestamp}' > "${local_location_output}"

    # 5. Download attachments if needed.
    if [ "${bucket}" = "blocklists" ] && [ "${collection}" = "addons-bloomfilters" ]; then
      # Find the attachment with the most recent generation_time, like _updateMLBF in Blocklist.jsm.
      # The server should return one "bloomfilter-base" record, but in case it returns multiple,
      # return the most recent one. The server may send multiple entries if we ever decide to use
      # the "filter_expression" feature of Remote Settings to send different records to specific
      # channels. In that case this code should be updated to recognize the filter expression,
      # but until we do, simply select the most recent record - can't go wrong with that.
      # Note that "attachment_type" and "generation_time" are specific to addons-bloomfilters.
      update_remote_settings_attachment "${bucket}" "${collection}" addons-mlbf.bin \
        'map(select(.attachment_type == "bloomfilter-base")) | sort_by(.generation_time) | last'
    fi
    # TODO: Bug 1873448. This cannot handle new/removed files currently, due to the
    # build system making it difficult.
    if [ "${bucket}" = "main" ] && [ "${collection}" = "search-config-icons" ]; then
      ${JQ} -r '.data[] | .id' < "${local_location_output}" |\
      while read -r id; do
        # We do not want quotes around ${id}
        # shellcheck disable=SC2086
        update_remote_settings_attachment "${bucket}" "${collection}" ${id} ".[] | select(.id == \"${id}\")"
      done
    fi
    # NOTE: The downloaded data is not validated. xpcshell should be used for that.
  done

  echo "INFO: diffing old/new remote settings dumps..."
  ${DIFF} -r "${REMOTE_SETTINGS_INPUT}" "${REMOTE_SETTINGS_OUTPUT}" > "${REMOTE_SETTINGS_DIFF_ARTIFACT}"
  if [ -s "${REMOTE_SETTINGS_DIFF_ARTIFACT}" ]
  then
    return 0
  fi
  return 1
}

# Helper for compare_remote_settings_files to download attachments from remote settings.
# The format and location is documented at:
# https://firefox-source-docs.mozilla.org/services/common/services/RemoteSettings.html#packaging-attachments
function update_remote_settings_attachment() {
  local bucket=$1
  local collection=$2
  local attachment_id=$3
  # $4 is a jq filter on the arrays that should return one record with the attachment
  local jq_attachment_selector=".data | map(select(.attachment)) | $4"

  # These paths match _readAttachmentDump in services/settings/Attachments.jsm.
  local path_to_attachment="${bucket}/${collection}/${attachment_id}"
  local path_to_meta="${bucket}/${collection}/${attachment_id}.meta.json"
  local old_meta="$REMOTE_SETTINGS_INPUT/${path_to_meta}"
  local new_meta="$REMOTE_SETTINGS_OUTPUT/${path_to_meta}"

  # Those files should have been created by compare_remote_settings_files before the function call.
  local local_location_input="$REMOTE_SETTINGS_INPUT/${bucket}/${collection}.json"
  local local_location_output="$REMOTE_SETTINGS_OUTPUT/${bucket}/${collection}.json"

  # Compute the metadata based on already-downloaded records.
  mkdir -p "$REMOTE_SETTINGS_INPUT/${bucket}/${collection}"
  ${JQ} -cj <"$local_location_input" "${jq_attachment_selector}" > "${old_meta}"
  mkdir -p "$REMOTE_SETTINGS_OUTPUT/${bucket}/${collection}"
  ${JQ} -cj <"$local_location_output" "${jq_attachment_selector}" > "${new_meta}"

  if cmp --silent "${old_meta}" "${new_meta}" ; then
    # Metadata not changed, don't bother downloading the attachments themselves.
    return
  fi
  # Metadata changed. Download attachments.

  echo "INFO: Downloading updated remote settings dump: ${bucket}/${collection}/${attachment_id}"

  # Overwrited old_meta with the actual file from the repo. The content should be equivalent,
  # but can have minor differences (e.g. different line endings) if the checked in file was not
  # generated by this script (e.g. manually checked in).
  ${WGET} -qO "${old_meta}" "${HGREPO}/raw-file/default${REMOTE_SETTINGS_DIR}/${path_to_meta}"

  ${WGET} -qO "${REMOTE_SETTINGS_INPUT}/${path_to_attachment}" "${HGREPO}/raw-file/default${REMOTE_SETTINGS_DIR}/${path_to_attachment}"

  if [ -z "${ATTACHMENT_BASE_URL}" ] ; then
    ATTACHMENT_BASE_URL=$(${WGET} -qO- "${REMOTE_SETTINGS_SERVER}" | ${JQ} -r .capabilities.attachments.base_url)
  fi
  attachment_path_from_meta=$(${JQ} -r < "${new_meta}" .attachment.location)
  ${WGET} -qO "${REMOTE_SETTINGS_OUTPUT}/${path_to_attachment}" "${ATTACHMENT_BASE_URL}${attachment_path_from_meta}"
}

function compare_mobile_experiments() {
  echo "INFO ${WGET} ${EXPERIMENTER_URL}"
  ${WGET} -O experiments.json "${EXPERIMENTER_URL}"
  ${WGET} -O fenix-experiments-old.json "${HGREPO}/raw-file/default/${FENIX_INITIAL_EXPERIMENTS}"
  ${WGET} -O focus-experiments-old.json "${HGREPO}/raw-file/default/${FOCUS_INITIAL_EXPERIMENTS}"

  # shellcheck disable=SC2016
  ${JQ} --arg APP_NAME fenix '{"data":map(select(.appName == $APP_NAME))}' < experiments.json > fenix-experiments-new.json
  # shellcheck disable=SC2016
  ${JQ} --arg APP_NAME focus_android '{"data":map(select(.appName == $APP_NAME))}' < experiments.json > focus-experiments-new.json

  ( ${DIFF} fenix-experiments-old.json fenix-experiments-new.json; ${DIFF} focus-experiments-old.json focus-experiments-new.json ) > "${EXPERIMENTER_DIFF_ARTIFACT}"
  if [ -s "${EXPERIMENTER_DIFF_ARTIFACT}" ]; then
    # no change
    return 1
  else
    return 0
  fi
}

# Clones an hg repo
function clone_repo {
  cd "${BASEDIR}"
  if [ ! -d "${REPODIR}" ]; then
    ${HG} robustcheckout --sharebase /tmp/hg-store -b default "${HGREPO}" "${REPODIR}"
  fi

  ${HG} -R "${REPODIR}" pull
  ${HG} -R "${REPODIR}" update -C default
}

# Copies new HSTS files in place, and commits them.
function stage_hsts_files {
  cd "${BASEDIR}"
  cp -f "${HSTS_PRELOAD_INC_NEW}" "${REPODIR}/security/manager/ssl/"
}

function stage_hpkp_files {
  cd "${BASEDIR}"
  cp -f "${HPKP_PRELOAD_OUTPUT}" "${REPODIR}/security/manager/ssl/${HPKP_PRELOAD_INC}"
}

function stage_remote_settings_files {
  cd "${BASEDIR}"
  cp -a "${REMOTE_SETTINGS_OUTPUT}"/* "${REPODIR}${REMOTE_SETTINGS_DIR}"
}

function stage_tld_suffix_files {
  cd "${BASEDIR}"
  cp -a "${GITHUB_SUFFIX_LOCAL}" "${REPODIR}/${HG_SUFFIX_PATH}"
}

function stage_mobile_experiments_files {
  cd "${BASEDIR}"

  cp fenix-experiments-new.json "${REPODIR}/${FENIX_INITIAL_EXPERIMENTS}"
  cp focus-experiments-new.json "${REPODIR}/${FOCUS_INITIAL_EXPERIMENTS}"
}

# Push all pending commits to Phabricator
function push_repo {
  cd "${REPODIR}"
  if [ ! -r "${HOME}/.arcrc" ]
  then
    return 1
  fi
  if ! ARC=$(command -v arc) && ! ARC=$(command -v arcanist)
  then
    return 1
  fi
  if [ -z "${REVIEWERS}" ]
  then
    return 1
  fi
  # Clean up older review requests
  # Turn  Needs Review D624: No bug, Automated HSTS ...
  # into D624
  for diff in $($ARC list | grep "Needs Review" | grep -E "${BRANCH} repo-update" | awk 'match($0, /D[0-9]+[^: ]/) { print substr($0, RSTART, RLENGTH)  }')
  do
    echo "Removing old request $diff"
    # There is no 'arc abandon', see bug 1452082
    echo '{"transactions": [{"type":"abandon", "value": true}], "objectIdentifier": "'"${diff}"'"}' | $ARC call-conduit -- differential.revision.edit
  done

  $ARC diff --verbatim --reviewers "${REVIEWERS}"
}



# Main

# Parse our command-line options.
while [ $# -gt 0 ]; do
  case "$1" in
    -h) usage; exit 0 ;;
    -p) PRODUCT="$2"; shift ;;
    -b) BRANCH="$2"; shift ;;
    -n) DRY_RUN=true ;;
    -c) CLOSED_TREE=true ;;
    -d) DONTBUILD=true ;;
    -a) APPROVAL=true ;;
    --pinset) DO_PRELOAD_PINSET=true ;;
    --hsts) DO_HSTS=true ;;
    --hpkp) DO_HPKP=true ;;
    --remote-settings) DO_REMOTE_SETTINGS=true ;;
    --suffix-list) DO_SUFFIX_LIST=true ;;
    --mobile-experiments) DO_MOBILE_EXPERIMENTS=true ;;
    -r) REPODIR="$2"; shift ;;
    --use-mozilla-central) USE_MC=true ;;
    --use-ftp-builds) USE_TC=false ;;
    -*) usage
      exit 11 ;;
    *)  break ;; # terminate while loop
  esac
  shift
done

# Must supply a code branch to work with.
if [ "${BRANCH}" == "" ]; then
  echo "Error: You must specify a branch with -b branchname." >&2
  usage
  exit 12
fi

# Must choose at least one update action.
if [ "$DO_HSTS" == "false" ] && [ "$DO_HPKP" == "false" ] && [ "$DO_REMOTE_SETTINGS" == "false" ] && [ "$DO_SUFFIX_LIST" == "false" ] && [ "$DO_MOBILE_EXPERIMENTS" == false ]
then
  echo "Error: you must specify at least one action from: --hsts, --hpkp, --remote-settings, or --suffix-list" >&2
  usage
  exit 13
fi

# per-product constants
case "${PRODUCT}" in
  thunderbird)
    COMMIT_AUTHOR="tbirdbld <tbirdbld@thunderbird.net>"
    ;;
  firefox)
    ;;
  *)
    echo "Error: Invalid product specified"
    usage
    exit 14
    ;;
esac

if [ "${REPODIR}" == "" ]; then
  REPODIR="$(basename "${BRANCH}")"
fi

case "${BRANCH}" in
  mozilla-central|comm-central|try )
    HGREPO="https://${HGHOST}/${BRANCH}"
    ;;
  mozilla-*|comm-* )
    HGREPO="https://${HGHOST}/releases/${BRANCH}"
    ;;
  * )
    HGREPO="https://${HGHOST}/projects/${BRANCH}"
    ;;
esac

BROWSER_ARCHIVE="target.${PLATFORM_EXT}"
TESTS_ARCHIVE="target.common.tests.tar.gz"

preflight_cleanup
if [ "${DO_HSTS}" == "true" ] || [ "${DO_HPKP}" == "true" ] || [ "${DO_PRELOAD_PINSET}" == "true" ]
then
  if [ "${USE_TC}" == "true" ]; then
    download_shared_artifacts_from_tc
  else
    download_shared_artifacts_from_ftp
  fi
  unpack_artifacts
fi

if [ "${DO_HSTS}" == "true" ]; then
  if compare_hsts_files
  then
    HSTS_UPDATED=true
  fi
fi
if [ "${DO_HPKP}" == "true" ]; then
  if compare_hpkp_files
  then
    HPKP_UPDATED=true
  fi
fi
if [ "${DO_REMOTE_SETTINGS}" == "true" ]; then
  if compare_remote_settings_files
  then
    REMOTE_SETTINGS_UPDATED=true
  fi
fi
if [ "${DO_SUFFIX_LIST}" == "true" ]; then
  if compare_suffix_lists
  then
    SUFFIX_LIST_UPDATED=true
  fi
fi
if [ "${DO_MOBILE_EXPERIMENTS}" == "true" ]; then
  if compare_mobile_experiments
  then
    MOBILE_EXPERIMENTS_UPDATED=true
  fi
fi


if [ "${HSTS_UPDATED}" == "false" ] && [ "${HPKP_UPDATED}" == "false" ] && [ "${REMOTE_SETTINGS_UPDATED}" == "false" ] && [ "${SUFFIX_LIST_UPDATED}" == "false" ] && [ "${MOBILE_EXPERIMENTS_UPDATED}" == "false" ]; then
  echo "INFO: no updates required. Exiting."
  exit 0
else
  if [ "${DRY_RUN}" == "true" ]; then
    echo "INFO: Updates are available, not updating hg in dry-run mode."
    exit 2
  fi
fi

clone_repo

COMMIT_MESSAGE="No Bug, ${BRANCH} repo-update"
if [ "${HSTS_UPDATED}" == "true" ]
then
  stage_hsts_files
  COMMIT_MESSAGE="${COMMIT_MESSAGE} HSTS"
fi

if [ "${HPKP_UPDATED}" == "true" ]
then
  stage_hpkp_files
  COMMIT_MESSAGE="${COMMIT_MESSAGE} HPKP"
fi

if [ "${REMOTE_SETTINGS_UPDATED}" == "true" ]
then
  stage_remote_settings_files
  COMMIT_MESSAGE="${COMMIT_MESSAGE} remote-settings"
fi

if [ "${SUFFIX_LIST_UPDATED}" == "true" ]
then
  stage_tld_suffix_files
  COMMIT_MESSAGE="${COMMIT_MESSAGE} tld-suffixes"
fi

if [ "${MOBILE_EXPERIMENTS_UPDATED}" == "true" ]
then
  stage_mobile_experiments_files
  COMMIT_MESSAGE="${COMMIT_MESSAGE} mobile-experiments"
fi

if [ ${DONTBUILD} == true ]; then
  COMMIT_MESSAGE="${COMMIT_MESSAGE} - (DONTBUILD)"
fi
if [ ${CLOSED_TREE} == true ]; then
  COMMIT_MESSAGE="${COMMIT_MESSAGE} - CLOSED TREE"
fi
if [ ${APPROVAL} == true ]; then
  COMMIT_MESSAGE="${COMMIT_MESSAGE} - a=repo-update"
fi


if ${HG} -R "${REPODIR}" commit -u "${COMMIT_AUTHOR}" -m "${COMMIT_MESSAGE}"
then
  push_repo
fi

echo "All done"
