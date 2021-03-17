#!/bin/bash

set -e

function usage {
  cat <<EOF

Usage: $(basename "$0") -h # Displays this usage/help text
Usage: $(basename "$0") -x # lists exit codes
Usage: $(basename "$0") -b branch -r REQUIREMENTS_FILE [-2] [-3]

EOF
}

BRANCH=""
PIP=""
COMMIT_AUTHOR='ffxbld <ffxbld@mozilla.com>'
REPODIR=''
HGHOST="hg.mozilla.org"
BASEDIR="${HOME}"
REQUIREMENTS_FILE=""

HG="$(command -v hg)"

# Clones an hg repo
function clone_repo {
  cd "${BASEDIR}"
  if [ ! -d "${REPODIR}" ]; then
    CLONE_CMD="${HG} clone ${HGREPO} ${REPODIR}"
    ${CLONE_CMD}
  fi

  ${HG} -R "${REPODIR}" pull
  ${HG} -R "${REPODIR}" update -C default
}

# Push all pending commits to Phabricator
function push_repo {
  cd "${REPODIR}"
  if [ ! -r "${HOME}/.arcrc" ]
  then
    return 1
  fi
  if ! ARC=$(command -v arc)
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
  for diff in $($ARC list | grep "Needs Review" | grep "${REQUIREMENTS_FILE} pip-update" | awk 'match($0, /D[0-9]+[^: ]/) { print substr($0, RSTART, RLENGTH)  }')
  do
    echo "Removing old request $diff"
    # There is no 'arc abandon', see bug 1452082
    echo '{"transactions": [{"type":"abandon"}], "objectIdentifier": "'"${diff}"'"}' | arc call-conduit differential.revision.edit
  done

  $ARC diff --verbatim --reviewers "${REVIEWERS}"
}

function update_requirements {
  pushd "${REPODIR}/${1}"
  pip-compile --generate-hashes "${2}"
  popd
}

# Main

# Parse our command-line options.
while [ $# -gt 0 ]; do
  case "$1" in
    -h) usage; exit 0 ;;
    -b) BRANCH="$2"; shift ;;
    -r) REPODIR="$2"; shift ;;
    -2) PIP="pip" ;;
    -3) PIP="pip3" ;;
    -f) REQUIREMENTS_FILE="$2"; shift ;;
    -*) usage
      exit 11 ;;
    *)  break ;; # terminate while loop
  esac
  shift
done

# Must supply a code branch to work with.
if [ "${PIP}" == "" ]; then
  echo "Error: You must specify a python version with -2 or -3" >&2
  usage
  exit 12
fi

# Must supply a code branch to work with.
if [ "${BRANCH}" == "" ]; then
  echo "Error: You must specify a branch with -b branchname." >&2
  usage
  exit 13
fi

if [ "${REPODIR}" == "" ]; then
  REPODIR="${BASEDIR}/$(basename "${BRANCH}")"
fi

if [ "${BRANCH}" == "mozilla-central" ]; then
  HGREPO="https://${HGHOST}/${BRANCH}"
elif [[ "${BRANCH}" == mozilla-* ]]; then
  HGREPO="https://${HGHOST}/releases/${BRANCH}"
else
  HGREPO="https://${HGHOST}/projects/${BRANCH}"
fi

clone_repo

requirements_basefile="$(basename "${REQUIREMENTS_FILE}")"
requirements_dir="$(dirname "${REQUIREMENTS_FILE}")"
update_requirements "${requirements_dir}" "${requirements_basefile}"
requirements_newfile="${requirements_basefile%%.in}.txt"
DIFF_ARTIFACT="${ARTIFACTS_DIR}/${requirements_newfile}.diff"

echo "INFO: diffing old/new ${requirements_newfile} into ${DIFF_ARTIFACT}"
${HG} -R "${REPODIR}" diff "${BASEDIR}/${BRANCH}/${requirements_dir}/${requirements_newfile}" | tee "${DIFF_ARTIFACT}"

COMMIT_MESSAGE="No Bug, ${requirements_dir}/${requirements_newfile} pip-update."

if ${HG} -R "${REPODIR}" commit -u "${COMMIT_AUTHOR}" -m "${COMMIT_MESSAGE}"
then
  ${HG} -R "${REPODIR}" out
  push_repo
fi

echo "All done"
