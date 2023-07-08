#!/bin/bash -ex

function usage() {
    echo "Usage: $0 [--project <js|browser>] <workspace-dir> flags..."
    echo "flags are treated the same way as a commit message would be"
    echo "(as in, they are scanned for directives just like a try: ... line)"
}

PROJECT=js
WORKSPACE=
while [[ $# -gt 0 ]]; do
    if [[ "$1" == "-h" ]] || [[ "$1" == "--help" ]]; then
        usage
        exit 0
    elif [[ "$1" == "--project" ]]; then
        shift
        PROJECT="$1"
        shift
    elif [[ "$1" == "--no-tooltool" ]]; then
        shift
    elif [[ -z "$WORKSPACE" ]]; then
        WORKSPACE=$( cd "$1" && pwd )
        shift
        break
    fi
done

function check_commit_msg () {
    ( set +e;
    if [[ -n "$AUTOMATION" ]]; then
        hg --cwd "$GECKO_PATH" log -r. --template '{desc}\n' | grep -F -q -- "$1"
    else
        echo -- "$SCRIPT_FLAGS" | grep -F -q -- "$1"
    fi
    )
}

if check_commit_msg "--dep"; then
    HAZ_DEP=1
fi

SCRIPT_FLAGS=$*

ANALYSIS_DIR="$WORKSPACE/haz-$PROJECT"

# Ensure all the scripts in this dir are on the path....
DIRNAME=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
PATH=$DIRNAME:$PATH

# Use GECKO_BASE_REPOSITORY as a signal for whether we are running in automation.
export AUTOMATION=${GECKO_BASE_REPOSITORY:+1}

: "${GECKO_PATH:="$DIRNAME"/../../..}"

if ! [ -d "$GECKO_PATH" ]; then
    echo "GECKO_PATH must be set to a directory containing a gecko source checkout" >&2
    exit 1
fi

# Directory to hold the compiled JS shell that will run the analysis.
HAZARD_SHELL_OBJDIR=$WORKSPACE/obj-haz-shell

export NO_MERCURIAL_SETUP_CHECK=1

if [[ "$PROJECT" = "browser" ]]; then (
    cd "$WORKSPACE"
    set "$WORKSPACE"
    # Mozbuild config:
    export MOZBUILD_STATE_PATH=$WORKSPACE/mozbuild/
    # Create .mozbuild so mach doesn't complain about this
    mkdir -p "$MOZBUILD_STATE_PATH"
) fi

# Build the shell
export HAZARD_SHELL_OBJDIR # This will be picked up by mozconfig.haz_shell.
$GECKO_PATH/mach hazards build-shell

# Run a self-test
$GECKO_PATH/mach hazards self-test --shell-objdir="$HAZARD_SHELL_OBJDIR"

# Artifacts folder is outside of the cache.
mkdir -p "$HOME"/artifacts/ || true

function grab_artifacts () {
    local artifacts
    artifacts="$HOME/artifacts"

    [ -d "$ANALYSIS_DIR" ] && (
        cd "$ANALYSIS_DIR"
        ls -lah

        # Do not error out if no files found
        shopt -s nullglob
        set +e
        local important
        important=(refs.txt unnecessary.txt hazards.txt gcFunctions.txt allFunctions.txt heapWriteHazards.txt rootingHazards.json hazards.html)

        # Bundle up the less important but still useful intermediate outputs,
        # just to cut down on the clutter in treeherder's Job Details pane.
        tar -acvf "${artifacts}/hazardIntermediates.tar.xz" --exclude-from <(IFS=$'\n'; echo "${important[*]}") *.txt *.lst build_xgill.log

        # Upload the important outputs individually, so that they will be
        # visible in Job Details and accessible to automated jobs.
        for f in "${important[@]}"; do
            gzip -9 -c "$f" > "${artifacts}/$f.gz"
        done

        # Check whether the user requested .xdb file upload in the top commit comment
        if check_commit_msg "--upload-xdbs"; then
            HAZ_UPLOAD_XDBS=1
        fi

        if [ -n "$HAZ_UPLOAD_XDBS" ]; then
            for f in *.xdb; do
                xz -c "$f" > "${artifacts}/$f.bz2"
            done
        fi
    )
}

function check_hazards () {
    (
    set +e
    NUM_HAZARDS=$(grep -c 'Function.*has unrooted.*live across GC call' "$1"/hazards.txt)
    NUM_UNSAFE=$(grep -c '^Function.*takes unsafe address of unrooted' "$1"/refs.txt)
    NUM_UNNECESSARY=$(grep -c '^Function.* has unnecessary root' "$1"/unnecessary.txt)
    NUM_DROPPED=$(grep -c '^Dropped CFG' "$1"/build_xgill.log)
    NUM_WRITE_HAZARDS=$(perl -lne 'print $1 if m!found (\d+)/\d+ allowed errors!' "$1"/heapWriteHazards.txt)
    NUM_MISSING=$(grep -c '^Function.*expected hazard.*but none were found' "$1"/hazards.txt)

    set +x
    echo "TinderboxPrint: rooting hazards<br/>$NUM_HAZARDS"
    echo "TinderboxPrint: (unsafe references to unrooted GC pointers)<br/>$NUM_UNSAFE"
    echo "TinderboxPrint: (unnecessary roots)<br/>$NUM_UNNECESSARY"
    echo "TinderboxPrint: missing expected hazards<br/>$NUM_MISSING"
    echo "TinderboxPrint: heap write hazards<br/>$NUM_WRITE_HAZARDS"

    # Display errors in a way that will get picked up by the taskcluster scraper.
    perl -lne 'print "TEST-UNEXPECTED-FAIL | hazards | $1 $2" if /^Function.* has (unrooted .*live across GC call).* (at .*)$/' "$1"/hazards.txt

    exit_status=0

    if [ $NUM_HAZARDS -gt 0 ]; then
        echo "TEST-UNEXPECTED-FAIL | hazards | $NUM_HAZARDS rooting hazards detected" >&2
        echo "TinderboxPrint: documentation<br/><a href='https://firefox-source-docs.mozilla.org/js/HazardAnalysis/#diagnosing-a-rooting-hazards-failure'>static rooting hazard analysis failures</a>, visit \"Inspect Task\" link for hazard details"
        exit_status=1
    fi

    if [ $NUM_MISSING -gt 0 ]; then
        echo "TEST-UNEXPECTED-FAIL | hazards | $NUM_MISSING expected hazards went undetected" >&2
        echo "TinderboxPrint: documentation<br/><a href='https://firefox-source-docs.mozilla.org/js/HazardAnalysis/#diagnosing-a-rooting-hazards-failure'>static rooting hazard analysis failures</a>, visit \"Inspect Task\" link for hazard details"
        exit_status=1
    fi

    NUM_ALLOWED_WRITE_HAZARDS=0
    if [ $NUM_WRITE_HAZARDS -gt $NUM_ALLOWED_WRITE_HAZARDS ]; then
        echo "TEST-UNEXPECTED-FAIL | heap-write-hazards | $NUM_WRITE_HAZARDS heap write hazards detected out of $NUM_ALLOWED_WRITE_HAZARDS allowed" >&2
        echo "TinderboxPrint: documentation<br/><a href='https://firefox-source-docs.mozilla.org/js/HazardAnalysis/#diagnosing-a-heap-write-hazard-failure'>heap write hazard analysis failures</a>, visit \"Inspect Task\" link for hazard details"
        exit_status = 1
    fi

    if [ $NUM_DROPPED -gt 0 ]; then
        echo "TEST-UNEXPECTED-FAIL | hazards | $NUM_DROPPED CFGs dropped" >&2
        echo "TinderboxPrint: sixgill unable to handle constructs<br/>$NUM_DROPPED"
        exit_status=1
    fi

    if [ $exit_status -ne 0 ]; then
        exit $exit_status
    fi
    )
}

trap grab_artifacts EXIT

# Gather the information from the source tree by compiling it.
$GECKO_PATH/mach hazards gather --project=$PROJECT --work-dir="$ANALYSIS_DIR"

# Analyze the collected information.
$GECKO_PATH/mach hazards analyze --project=$PROJECT --shell-objdir="$HAZARD_SHELL_OBJDIR" --work-dir="$ANALYSIS_DIR"

check_hazards "$ANALYSIS_DIR"

################################### script end ###################################
