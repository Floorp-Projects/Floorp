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
           --hsts | --hpkp | --blocklist
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
HG_SSH_USER='ffxbld'
REPODIR=''
APP_DIR=''
APP_ID=''
APP_NAME=''
HGHOST="hg.mozilla.org"
STAGEHOST="archive.mozilla.org"
WGET="wget -nv"
UNZIP="unzip -q"
DIFF="$(which diff) -u"
BASEDIR="${HOME}"
TOOLSDIR="${HOME}/tools"

SCRIPTDIR="$(realpath "$(dirname "$0")")"
HG="$(which hg)"
DATADIR="${BASEDIR}/data"
mkdir -p "${DATADIR}"

VERSION=''
MCVERSION=''
USE_MC=false
USE_TC=true
JQ="$(which jq)"

DO_HSTS=false
HSTS_PRELOAD_SCRIPT="${SCRIPTDIR}/getHSTSPreloadList.js"
HSTS_PRELOAD_ERRORS="nsSTSPreloadList.errors"
HSTS_PRELOAD_INC="${DATADIR}/nsSTSPreloadList.inc"
HSTS_UPDATED=false

DO_HPKP=false
HPKP_PRELOAD_SCRIPT="${SCRIPTDIR}/genHPKPStaticPins.js"
HPKP_PRELOAD_ERRORS="StaticHPKPins.errors"
HPKP_PRELOAD_JSON="${DATADIR}/PreloadedHPKPins.json"
HPKP_PRELOAD_INC="StaticHPKPins.h"
HPKP_PRELOAD_INPUT="${DATADIR}/${HPKP_PRELOAD_INC}"
HPKP_PRELOAD_OUTPUT="${DATADIR}/${HPKP_PRELOAD_INC}.out"
HPKP_UPDATED=false

DO_BLOCKLIST=false
BLOCKLIST_URL_AMO=''
BLOCKLIST_URL_HG=''
BLOCKLIST_LOCAL_AMO="blocklist_amo.xml"
BLOCKLIST_LOCAL_HG="blocklist_hg.xml"
BLOCKLIST_UPDATED=false

DO_REMOTE_SETTINGS=false
REMOTE_SETTINGS_SERVER=''
REMOTE_SETTINGS_INPUT="${DATADIR}/remote-settings.in"
REMOTE_SETTINGS_OUTPUT="${DATADIR}/remote-settings.out"
REMOTE_SETTINGS_DIR="/services/settings/dumps"
REMOTE_SETTINGS_UPDATED=false

ARTIFACTS_DIR="${ARTIFACTS_DIR:-.}"
# Defaults
HSTS_DIFF_ARTIFACT="${ARTIFACTS_DIR}/${HSTS_DIFF_ARTIFACT:-"nsSTSPreloadList.diff"}"
HPKP_DIFF_ARTIFACT="${ARTIFACTS_DIR}/${HPKP_DIFF_ARTIFACT:-"StaticHPKPins.h.diff"}"
BLOCKLIST_DIFF_ARTIFACT="${ARTIFACTS_DIR}/${BLOCKLIST_DIFF_ARTIFACT:-"blocklist.diff"}"
REMOTE_SETTINGS_DIFF_ARTIFACT="${ARTIFACTS_DIR}/${REMOTE_SETTINGS_DIFF_ARTIFACT:-"remote-settings.diff"}"


# Get the current in-tree version for a code branch.
function get_version {
  VERSION_REPO=$1
  VERSION_FILE='version.txt'

  # TODO bypass temporary file

  cd "${BASEDIR}"
  VERSION_URL_HG="${VERSION_REPO}/raw-file/default/${APP_DIR}/config/version.txt"
  rm -f ${VERSION_FILE}
  ${WGET} -O "${VERSION_FILE}" "${VERSION_URL_HG}"
  PARSED_VERSION=$(cat version.txt)
  if [ "${PARSED_VERSION}" == "" ]; then
    echo "ERROR: Unable to parse version from $VERSION_FILE" >&2
    exit 21
  fi
  rm -f ${VERSION_FILE}
  echo "${PARSED_VERSION}"
}

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
  TASKID_URL="https://index.taskcluster.net/v1/task/gecko.v2.${REPODIR}.latest.${PRODUCT}.linux64-opt"
  if [ "${USE_MC}" == "true" ]; then
    TASKID_URL="https://index.taskcluster.net/v1/task/gecko.v2.mozilla-central.latest.${PRODUCT}.linux64-opt"
  fi
  ${WGET} -O ${TASKID_FILE} ${TASKID_URL}
  INDEX_TASK_ID="$($JQ -r '.taskId' ${TASKID_FILE})"
  if [ -z "${INDEX_TASK_ID}" ]; then
    echo "Failed to look up taskId at ${TASKID_URL}"
    exit 22
  else
    echo "INFO: Got taskId of $INDEX_TASK_ID"
  fi

  TASKSTATUS_FILE="taskstatus.json"
  STATUS_URL="https://queue.taskcluster.net/v1/task/${INDEX_TASK_ID}/status"
  ${WGET} -O "${TASKSTATUS_FILE}" "${STATUS_URL}"
  LAST_RUN_INDEX=$(($(jq '.status.runs | length' ${TASKSTATUS_FILE}) - 1))
  echo "INFO: Examining run number ${LAST_RUN_INDEX}"

  BROWSER_ARCHIVE_URL="https://queue.taskcluster.net/v1/task/${INDEX_TASK_ID}/runs/${LAST_RUN_INDEX}/artifacts/public/build/${BROWSER_ARCHIVE}"
  echo "INFO: ${WGET} ${BROWSER_ARCHIVE_URL}"
  ${WGET} "${BROWSER_ARCHIVE_URL}"

  TESTS_ARCHIVE_URL="https://queue.taskcluster.net/v1/task/${INDEX_TASK_ID}/runs/${LAST_RUN_INDEX}/artifacts/public/build/${TESTS_ARCHIVE}"
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
  ${UNZIP} "../${TESTS_ARCHIVE}"
  cd "${BASEDIR}"
  cp tests/bin/xpcshell "${PRODUCT}"
}

# Downloads the current in-tree HSTS (HTTP Strict Transport Security) files.
# Runs a simple xpcshell script to generate up-to-date HSTS information.
# Compares the new HSTS output with the old to determine whether we need to update.
function compare_hsts_files {
  cd "${BASEDIR}"

  HSTS_PRELOAD_INC_HG="${HGREPO}/raw-file/default/security/manager/ssl/$(basename "${HSTS_PRELOAD_INC}")"

  echo "INFO: Downloading existing include file..."
  rm -rf "${HSTS_PRELOAD_ERRORS}" "${HSTS_PRELOAD_INC}"
  echo "INFO: ${WGET} ${HSTS_PRELOAD_INC_HG}"
  ${WGET} -O "${HSTS_PRELOAD_INC}" "${HSTS_PRELOAD_INC_HG}"

  if [ ! -f "${HSTS_PRELOAD_INC}" ]; then
    echo "Downloaded file '${HSTS_PRELOAD_INC}' not found in directory '$(pwd)' - this should have been downloaded above from ${HSTS_PRELOAD_INC_HG}." >&2
    exit 41
  fi

  # Run the script to get an updated preload list.
  echo "INFO: Generating new HSTS preload list..."
  cd "${BASEDIR}/${PRODUCT}"
  LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:. ./xpcshell "${HSTS_PRELOAD_SCRIPT}" "${HSTS_PRELOAD_INC}"

  # The created files should be non-empty.
  echo "INFO: Checking whether new HSTS preload list is valid..."
  if [ ! -s "${HSTS_PRELOAD_INC}" ]; then
    echo "New HSTS preload list ${HSTS_PRELOAD_INC} is empty. That's less good." >&2
    exit 42
  fi
  cd "${BASEDIR}"

  # Check for differences
  echo "INFO: diffing old/new HSTS preload lists into ${HSTS_DIFF_ARTIFACT}"
  ${DIFF} "${BASEDIR}/${PRODUCT}/$(basename "${HSTS_PRELOAD_INC}")" "${HSTS_PRELOAD_INC}" | tee "${HSTS_DIFF_ARTIFACT}"
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
  LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:. ./xpcshell "${HPKP_PRELOAD_SCRIPT}" "${HPKP_PRELOAD_JSON}" "${HPKP_PRELOAD_OUTPUT}" > "${HPKP_PRELOAD_ERRORS}"

  # The created files should be non-empty.
  echo "INFO: Checking whether new HPKP preload list is valid..."

  if [ ! -s "${HPKP_PRELOAD_OUTPUT}" ]; then
    echo "${HPKP_PRELOAD_OUTPUT} is empty. That's less good." >&2
    exit 52
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
  XMLLINT=$(which xmllint 2>/dev/null | head -n1)

  if [ ! -x "${XMLLINT}" ]; then
    echo "ERROR: xmllint not found in PATH"
    exit 60
  fi
  ${XMLLINT} --nonet --noout "${xmlfile}"
}

# Downloads the current in-tree blocklist file.
# Downloads the current blocklist file from AMO.
# Compares the AMO blocklist with the in-tree blocklist to determine whether we need to update.
function compare_blocklist_files {
  BLOCKLIST_URL_AMO="https://blocklist.addons.mozilla.org/blocklist/3/${APP_ID}/${VERSION}/${APP_NAME}/20090105024647/blocklist-sync/en-US/nightly/blocklist-sync/default/default/"
  BLOCKLIST_URL_HG="${HGREPO}/raw-file/default/${APP_DIR}/app/blocklist.xml"

  cd "${BASEDIR}"
  rm -f ${BLOCKLIST_LOCAL_AMO}
  echo "INFO: ${WGET} -O ${BLOCKLIST_LOCAL_AMO} ${BLOCKLIST_URL_AMO}"
  ${WGET} -O "${BLOCKLIST_LOCAL_AMO}" "${BLOCKLIST_URL_AMO}"

  rm -f ${BLOCKLIST_LOCAL_HG}
  echo "INFO: ${WGET} -O ${BLOCKLIST_LOCAL_HG} ${BLOCKLIST_URL_HG}"
  ${WGET} -O "${BLOCKLIST_LOCAL_HG}" "${BLOCKLIST_URL_HG}"

  # The downloaded files should be non-empty and have a valid xml header
  # if they were retrieved properly, and some random HTML garbage if not.
  # set -x catches these
  is_valid_xml ${BLOCKLIST_LOCAL_AMO}
  is_valid_xml ${BLOCKLIST_LOCAL_HG}

  echo "INFO: diffing in-tree blocklist against the blocklist from AMO..."
  ${DIFF} ${BLOCKLIST_LOCAL_HG} ${BLOCKLIST_LOCAL_AMO} | tee "${BLOCKLIST_DIFF_ARTIFACT}"
  if [ -s "${BLOCKLIST_DIFF_ARTIFACT}" ]
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
      ${JQ} -r '.data[] | .bucket+"/"+.collection' |\
      # 2. For each entry ${bucket, collection}
      while IFS="/" read -r bucket collection; do

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
        remote_records_url="$REMOTE_SETTINGS_SERVER/buckets/${bucket}/collections/${collection}/records"
        local_location_output="$REMOTE_SETTINGS_OUTPUT/${bucket}/${collection}.json"
        mkdir -p "$REMOTE_SETTINGS_OUTPUT/${bucket}"
        ${WGET} -qO "$local_location_output" "$remote_records_url"
      done

  echo "INFO: diffing old/new remote settings dumps..."
  ${DIFF} -r "${REMOTE_SETTINGS_INPUT}" "${REMOTE_SETTINGS_OUTPUT}" > "${REMOTE_SETTINGS_DIFF_ARTIFACT}"
  if [ -s "${REMOTE_SETTINGS_DIFF_ARTIFACT}" ]
  then
    return 0
  fi
  return 1
}

function clone_build_tools {
    rm -fr "${TOOLSDIR}"
    CLONE_CMD="${HG} clone https://hg.mozilla.org/build/tools ${TOOLSDIR}"
    ${CLONE_CMD}
}

# Clones an hg repo
function clone_repo {
  cd "${BASEDIR}"
  if [ ! -d "${REPODIR}" ]; then
    CLONE_CMD="${HG} clone ${HGREPO} ${REPODIR}"
    ${CLONE_CMD}
  fi

  ${HG} -R ${REPODIR} pull
  ${HG} -R ${REPODIR} update -C default
}

# Copies new HSTS files in place, and commits them.
function commit_hsts_files {
  cd "${BASEDIR}"

  cp -f "${BASEDIR}/${PRODUCT}/$(basename "${HSTS_PRELOAD_INC}")" "${REPODIR}/security/manager/ssl/"

  COMMIT_MESSAGE="No bug, Automated HSTS preload list update"
  if [ -n "${TASK_ID}" ]; then
    COMMIT_MESSAGE="${COMMIT_MESSAGE} from task ${TASK_ID}"
  fi
  if [ ${DONTBUILD} == true ]; then
    COMMIT_MESSAGE="${COMMIT_MESSAGE} - (DONTBUILD)"
  fi
  if [ ${CLOSED_TREE} == true ]; then
    COMMIT_MESSAGE="${COMMIT_MESSAGE} - CLOSED TREE"
  fi
  if [ ${APPROVAL} == true ]; then
    COMMIT_MESSAGE="${COMMIT_MESSAGE} - a=hsts-update"
  fi
  echo "INFO: committing HSTS changes"
  ${HG} -R ${REPODIR} commit -u "${HG_SSH_USER}" -m "${COMMIT_MESSAGE}"
}

# Copies new HPKP files in place, and commits them.
function commit_hpkp_files {
  cd "${BASEDIR}"

  cp -f "${HPKP_PRELOAD_OUTPUT}" "${REPODIR}/security/manager/ssl/${HPKP_PRELOAD_INC}"

  COMMIT_MESSAGE="No bug, Automated HPKP preload list update"
  if [ -n "${TASK_ID}" ]; then
    COMMIT_MESSAGE="${COMMIT_MESSAGE} from task ${TASK_ID}"
  fi
  if [ ${DONTBUILD} == true ]; then
    COMMIT_MESSAGE="${COMMIT_MESSAGE} - (DONTBUILD)"
  fi
  if [ ${CLOSED_TREE} == true ]; then
    COMMIT_MESSAGE="${COMMIT_MESSAGE} - CLOSED TREE"
  fi
  if [ ${APPROVAL} == true ]; then
    COMMIT_MESSAGE="${COMMIT_MESSAGE} - a=hpkp-update"
  fi
  echo "INFO: committing HPKP changes"
  ${HG} -R ${REPODIR} commit -u "${HG_SSH_USER}" -m "${COMMIT_MESSAGE}"
}

# Copies new blocklist file in place, and commits it.
function commit_blocklist_files {
  cd "${BASEDIR}"
  cp -f ${BLOCKLIST_LOCAL_AMO} ${REPODIR}/${APP_DIR}/app/blocklist.xml
  COMMIT_MESSAGE="No bug, Automated blocklist update"
  if [ -n "${TASK_ID}" ]; then
    COMMIT_MESSAGE="${COMMIT_MESSAGE} from task ${TASK_ID}"
  fi
  if [ ${DONTBUILD} == true ]; then
    COMMIT_MESSAGE="${COMMIT_MESSAGE} - (DONTBUILD)"
  fi
  if [ ${CLOSED_TREE} == true ]; then
    COMMIT_MESSAGE="${COMMIT_MESSAGE} - CLOSED TREE"
  fi
  if [ ${APPROVAL} == true ]; then
    COMMIT_MESSAGE="${COMMIT_MESSAGE} - a=blocklist-update"
  fi
  echo "INFO: committing blocklist changes"
  ${HG} -R ${REPODIR} commit -u "${HG_SSH_USER}" -m "${COMMIT_MESSAGE}"
}

# Copies new remote settings dump files in place, and commits them.
function commit_remote_settings_files {
  cd "${BASEDIR}"
  cp -a "${REMOTE_SETTINGS_OUTPUT}"/* "${REPODIR}${REMOTE_SETTINGS_DIR}"

  COMMIT_MESSAGE="No bug, Automated remote settings update"
  if [ -n "${TASK_ID}" ]; then
    COMMIT_MESSAGE="${COMMIT_MESSAGE} from task ${TASK_ID}"
  fi
  if [ ${DONTBUILD} == true ]; then
    COMMIT_MESSAGE="${COMMIT_MESSAGE} - (DONTBUILD)"
  fi
  if [ ${CLOSED_TREE} == true ]; then
    COMMIT_MESSAGE="${COMMIT_MESSAGE} - CLOSED TREE"
  fi
  if [ ${APPROVAL} == true ]; then
    COMMIT_MESSAGE="${COMMIT_MESSAGE} - a=remote-settings-update"
  fi
  echo "INFO: committing remote settings changes"
  ${HG} -R ${REPODIR} commit -u "${HG_SSH_USER}" -m "${COMMIT_MESSAGE}"
}

# Push all pending commits to Phabricator
function push_repo {
  cd "${REPODIR}"
  if [ ! -r "${HOME}/.arcrc" ]
  then
    return 1
  fi
  if ! ARC=$(which arc)
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
  for diff in $($ARC list | grep "Needs Review" | grep -E "Automated HSTS|Automated HPKP|Automated blocklist" | awk 'match($0, /D[0-9]+[^: ]/) { print substr($0, RSTART, RLENGTH)  }')
  do
    echo "Removing old request $diff"
    # There is no 'arc abandon', see bug 1452082
    echo '{"transactions": [{"type":"abandon"}], "objectIdentifier": "'"${diff}"'"}' | arc call-conduit differential.revision.edit
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
    --blocklist) DO_BLOCKLIST=true ;;
    --remote-settings) DO_REMOTE_SETTINGS=true ;;
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
if [ "$DO_HSTS" == "false" ] && [ "$DO_HPKP" == "false" ] && [ "$DO_BLOCKLIST" == "false" ] && [ "$DO_REMOTE_SETTINGS" == "false" ]
then
  echo "Error: you must specify at least one action from: --hsts, --hpkp, --blocklist, --remote-settings" >&2
  usage
  exit 13
fi

# per-product constants
case "${PRODUCT}" in
  thunderbird)
    APP_DIR="mail"
    APP_ID="%7B3550f703-e582-4d05-9a08-453d09bdfdc6%7D"
    APP_NAME="Thunderbird"
    ;;
  firefox)
    APP_DIR="browser"
    APP_ID="%7Bec8030f7-c20a-464f-9b0e-13a3a9e97384%7D"
    APP_NAME="Firefox"
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

HGREPO="https://${HGHOST}/${BRANCH}"
MCREPO="https://${HGHOST}/mozilla-central"

# Remove once 52esr is off support
VERSION=$(get_version "${HGREPO}")
MAJOR_VERSION="${VERSION%.*}"
echo "INFO: parsed version is ${VERSION}"
if [ "${USE_MC}" == "true" ]; then
  MCVERSION=$(get_version "${MCREPO}")
  echo "INFO: parsed mozilla-central version is ${MCVERSION}"
  MAJOR_VERSION="${MCVERSION%.*}"
fi

BROWSER_ARCHIVE="${PRODUCT}-${VERSION}.en-US.${PLATFORM}.${PLATFORM_EXT}"
TESTS_ARCHIVE="${PRODUCT}-${VERSION}.en-US.${PLATFORM}.common.tests.zip"
if [ "${USE_MC}" == "true" ]; then
    BROWSER_ARCHIVE="${PRODUCT}-${MCVERSION}.en-US.${PLATFORM}.${PLATFORM_EXT}"
    TESTS_ARCHIVE="${PRODUCT}-${MCVERSION}.en-US.${PLATFORM}.common.tests.zip"
fi
# Simple name builds on >=53.0.0
if [ "${MAJOR_VERSION}" -ge 53 ] ; then
    BROWSER_ARCHIVE="target.${PLATFORM_EXT}"
    TESTS_ARCHIVE="target.common.tests.zip"
fi
# End 'remove once 52esr is off support'

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
if [ "${DO_BLOCKLIST}" == "true" ]; then
  if compare_blocklist_files
  then
    BLOCKLIST_UPDATED=true
  fi
fi
if [ "${DO_REMOTE_SETTINGS}" == "true" ]; then
  if compare_remote_settings_files
  then
    REMOTE_SETTINGS_UPDATED=true
  fi
fi

if [ "${HSTS_UPDATED}" == "false" ] && [ "${HPKP_UPDATED}" == "false" ] && [ "${BLOCKLIST_UPDATED}" == "false" ] && [ "${REMOTE_SETTINGS_UPDATED}" == "false" ]; then
  echo "INFO: no updates required. Exiting."
  exit 0
else
  if [ "${DRY_RUN}" == "true" ]; then
    echo "INFO: Updates are available, not updating hg in dry-run mode."
    exit 2
  fi
fi

# Currently less reliable than regular 'hg'
# clone_build_tools

clone_repo

MUST_PUSH=false
if [ "${HSTS_UPDATED}" == "true" ]
then
  commit_hsts_files
  MUST_PUSH=true
fi

if [ "${HPKP_UPDATED}" == "true" ]
then
  commit_hpkp_files
  MUST_PUSH=true
fi

if [ "${BLOCKLIST_UPDATED}" == "true" ]
then
  commit_blocklist_files
  MUST_PUSH=true
fi

if [ "${REMOTE_SETTINGS_UPDATED}" == "true" ]
then
  commit_remote_settings_files
  MUST_PUSH=true
fi

if [ -n "${MUST_PUSH}" ]
then
  push_repo
fi

echo "All done"
