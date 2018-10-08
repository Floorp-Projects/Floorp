#!/bin/bash

set -e

function usage {
  cat <<EOF

Usage: $(basename "$0") -h # Displays this usage/help text
Usage: $(basename "$0") -x # lists exit codes
Usage: $(basename "$0") -b branch -p pipfile_directory [-2] [-3]

EOF
}

BRANCH=""
PIP=""
COMMIT_AUTHOR='ffxbld <ffxbld@mozilla.com>'
REPODIR=''
HGHOST="hg.mozilla.org"
BASEDIR="${HOME}"
PIPFILE_DIRECTORY=""
DIFF_ARTIFACT="${ARTIFACTS_DIR}/Pipfile.lock.diff"

HG="$(command -v hg)"

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
  for diff in $($ARC list | grep "Needs Review" | grep "${PIPFILE_DIRECTORY} pipfile-update" | awk 'match($0, /D[0-9]+[^: ]/) { print substr($0, RSTART, RLENGTH)  }')
  do
    echo "Removing old request $diff"
    # There is no 'arc abandon', see bug 1452082
    echo '{"transactions": [{"type":"abandon"}], "objectIdentifier": "'"${diff}"'"}' | arc call-conduit differential.revision.edit
  done

  $ARC diff --verbatim --reviewers "${REVIEWERS}"
}

function update_pipfile {
  pushd "${REPODIR}/${1}"
  pipenv update
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
    -p) PIPFILE_DIRECTORY="$2"; shift ;;
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
  REPODIR="$(basename "${BRANCH}")"
fi

if [ "${BRANCH}" == "mozilla-central" ]; then
  HGREPO="https://${HGHOST}/${BRANCH}"
elif [[ "${BRANCH}" == mozilla-* ]]; then
  HGREPO="https://${HGHOST}/releases/${BRANCH}"
else
  HGREPO="https://${HGHOST}/projects/${BRANCH}"
fi

clone_repo

${PIP} install pipenv
# Bug 1497162
# Can be removed when https://github.com/pypa/pipenv/issues/2924 is released
${PIP} install --user pip==18.0

update_pipfile "${PIPFILE_DIRECTORY}"
echo "INFO: diffing old/new Pipfile.lock into ${DIFF_ARTIFACT}"
hg -R "${REPODIR}" diff "${BASEDIR}/${BRANCH}/${PIPFILE_DIRECTORY}/Pipfile.lock" | tee "${DIFF_ARTIFACT}"

COMMIT_MESSAGE="No Bug, ${PIPFILE_DIRECTORY} pipfile-update."

if ${HG} -R "${REPODIR}" commit -u "${COMMIT_AUTHOR}" -m "${COMMIT_MESSAGE}"
then
  ${HG} -R "${REPODIR}" out
  push_repo
fi

echo "All done"
