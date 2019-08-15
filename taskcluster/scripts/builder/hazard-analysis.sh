#!/bin/bash -ex

[ -n "$WORKSPACE" ]
[ -n "$MOZ_OBJDIR" ]
[ -n "$GECKO_PATH" ]

HAZARD_SHELL_OBJDIR=$WORKSPACE/obj-haz-shell
JSBIN="$HAZARD_SHELL_OBJDIR/dist/bin/js"
JS_SRCDIR=$GECKO_PATH/js/src
ANALYSIS_SRCDIR=$JS_SRCDIR/devtools/rootAnalysis
GCCDIR="$MOZ_FETCHES_DIR/gcc"

export CC="$GCCDIR/bin/gcc"
export CXX="$GCCDIR/bin/g++"
export PATH="$GCCDIR/bin:$MOZ_FETCHES_DIR/clang/bin:$PATH"
export LD_LIBRARY_PATH="$GCCDIR/lib64"
export RUSTC="$MOZ_FETCHES_DIR/rustc/bin/rustc"
export CARGO="$MOZ_FETCHES_DIR/rustc/bin/cargo"

PYTHON=python2.7
if ! which $PYTHON; then
    PYTHON=python
fi

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

function build_js_shell () {
    # Must unset MOZ_OBJDIR and MOZCONFIG here to prevent the build system from
    # inferring that the analysis output directory is the current objdir. We
    # need a separate objdir here to build the opt JS shell to use to run the
    # analysis.
    (
    unset MOZ_OBJDIR
    unset MOZCONFIG
    cp -P $JS_SRCDIR/configure.in $JS_SRCDIR/configure
    chmod +x $JS_SRCDIR/configure
    if [[ -z "$HAZ_DEP" ]]; then
        [ -d $HAZARD_SHELL_OBJDIR ] && rm -rf $HAZARD_SHELL_OBJDIR
    fi
    mkdir -p $HAZARD_SHELL_OBJDIR || true
    cd $HAZARD_SHELL_OBJDIR
    $JS_SRCDIR/configure --enable-optimize --disable-debug --enable-ctypes --enable-nspr-build --without-intl-api
    make -j$(nproc)
    ) # Restore MOZ_OBJDIR and MOZCONFIG
}

function configure_analysis () {
    local analysis_dir
    analysis_dir="$1"

    if [[ -z "$HAZ_DEP" ]]; then
        [ -d "$analysis_dir" ] && rm -rf "$analysis_dir"
    fi

    mkdir -p "$analysis_dir" || true
    (
        cd "$analysis_dir"
        cat > defaults.py <<EOF
js = "$JSBIN"
analysis_scriptdir = "$ANALYSIS_SRCDIR"
objdir = "$MOZ_OBJDIR"
source = "$GECKO_PATH"
sixgill = "$MOZ_FETCHES_DIR/sixgill/usr/libexec/sixgill"
sixgill_bin = "$MOZ_FETCHES_DIR/sixgill/usr/bin"
EOF

        local rev
        rev=$(cd $GECKO_PATH && hg log -r . -T '{node|short}')
        cat > run-analysis.sh <<EOF
#!/bin/sh
if [ \$# -eq 0 ]; then
  set gcTypes
fi
export ANALYSIS_SCRIPTDIR="$ANALYSIS_SRCDIR"
export URLPREFIX="https://hg.mozilla.org/mozilla-unified/file/$rev/"
exec "$ANALYSIS_SRCDIR/analyze.py" "\$@"
EOF
        chmod +x run-analysis.sh
    )
}

function run_analysis () {
    local analysis_dir
    analysis_dir="$1"
    local build_type
    build_type="$2"

    if [[ -z "$HAZ_DEP" ]]; then
        [ -d $MOZ_OBJDIR ] && rm -rf $MOZ_OBJDIR
    fi

    (
        cd "$analysis_dir"
        $PYTHON "$ANALYSIS_SRCDIR/analyze.py" -v --buildcommand="$GECKO_PATH/taskcluster/scripts/builder/hazard-${build_type}.sh"
    )
}

function analysis_self_test () {
    LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$(dirname "$JSBIN")" $PYTHON "$ANALYSIS_SRCDIR/run-test.py" -v --js "$JSBIN" --sixgill "$MOZ_FETCHES_DIR/sixgill" --gccdir "$GCCDIR"
}

function grab_artifacts () {
    local analysis_dir
    analysis_dir="$1"
    local artifacts
    artifacts="$2"

    (
        cd "$analysis_dir"
        ls -lah

        # Do not error out if no files found
        shopt -s nullglob
        set +e
        local important
        important=(refs.txt unnecessary.txt hazards.txt gcFunctions.txt allFunctions.txt heapWriteHazards.txt)

        # Bundle up the less important but still useful intermediate outputs,
        # just to cut down on the clutter in treeherder's Job Details pane.
        tar -acvf "${artifacts}/hazardIntermediates.tar.xz" --exclude-from <(for f in "${important[@]}"; do echo $f; done) *.txt *.lst build_xgill.log

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
                bzip2 -c "$f" > "${artifacts}/$f.bz2"
            done
        fi
    )
}

function check_hazards () {
    (
    set +e
    NUM_HAZARDS=$(grep -c 'Function.*has unrooted.*live across GC call' "$1"/rootingHazards.txt)
    NUM_UNSAFE=$(grep -c '^Function.*takes unsafe address of unrooted' "$1"/refs.txt)
    NUM_UNNECESSARY=$(grep -c '^Function.* has unnecessary root' "$1"/unnecessary.txt)
    NUM_DROPPED=$(grep -c '^Dropped CFG' "$1"/build_xgill.log)
    NUM_WRITE_HAZARDS=$(perl -lne 'print $1 if m!found (\d+)/\d+ allowed errors!' "$1"/heapWriteHazards.txt)
    NUM_MISSING=$(grep -c '^Function.*expected hazard.*but none were found' "$1"/rootingHazards.txt)

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
        echo "TinderboxPrint: documentation<br/><a href='https://wiki.mozilla.org/Javascript:Hazard_Builds#Diagnosing_a_rooting_hazards_failure'>static rooting hazard analysis failures</a>, visit \"Inspect Task\" link for hazard details"
        exit_status=1
    fi

    if [ $NUM_MISSING -gt 0 ]; then
        echo "TEST-UNEXPECTED-FAIL | hazards | $NUM_MISSING expected hazards went undetected" >&2
        echo "TinderboxPrint: documentation<br/><a href='https://wiki.mozilla.org/Javascript:Hazard_Builds#Diagnosing_a_rooting_hazards_failure'>static rooting hazard analysis failures</a>, visit \"Inspect Task\" link for hazard details"
        exit_status=1
    fi

    NUM_ALLOWED_WRITE_HAZARDS=0
    if [ $NUM_WRITE_HAZARDS -gt $NUM_ALLOWED_WRITE_HAZARDS ]; then
        echo "TEST-UNEXPECTED-FAIL | heap-write-hazards | $NUM_WRITE_HAZARDS heap write hazards detected out of $NUM_ALLOWED_WRITE_HAZARDS allowed" >&2
        echo "TinderboxPrint: documentation<br/><a href='https://wiki.mozilla.org/Javascript:Hazard_Builds#Diagnosing_a_heap_write_hazard_failure'>heap write hazard analysis failures</a>, visit \"Inspect Task\" link for hazard details"
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
