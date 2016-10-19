#!/bin/bash -ex

[ -n "$WORKSPACE" ]
[ -n "$MOZ_OBJDIR" ]
[ -n "$GECKO_DIR" ]

HAZARD_SHELL_OBJDIR=$WORKSPACE/obj-haz-shell
JS_SRCDIR=$GECKO_DIR/js/src
ANALYSIS_SRCDIR=$JS_SRCDIR/devtools/rootAnalysis

export CC="$TOOLTOOL_DIR/gcc/bin/gcc"
export CXX="$TOOLTOOL_DIR/gcc/bin/g++"

PYTHON=python2.7
if ! which $PYTHON; then
    PYTHON=python
fi


function check_commit_msg () {
    ( set +e;
    if [[ -n "$AUTOMATION" ]]; then
        hg --cwd "$GECKO_DIR" log -r. --template '{desc}\n' | grep -F -q -- "$1"
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
    ( cd $JS_SRCDIR; autoconf-2.13 )
    if [[ -z "$HAZ_DEP" ]]; then
        [ -d $HAZARD_SHELL_OBJDIR ] && rm -rf $HAZARD_SHELL_OBJDIR
    fi
    mkdir -p $HAZARD_SHELL_OBJDIR || true
    cd $HAZARD_SHELL_OBJDIR
    $JS_SRCDIR/configure --enable-optimize --disable-debug --enable-ctypes --enable-nspr-build --without-intl-api --with-ccache
    make -j4
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
js = "$HAZARD_SHELL_OBJDIR/dist/bin/js"
analysis_scriptdir = "$ANALYSIS_SRCDIR"
objdir = "$MOZ_OBJDIR"
source = "$GECKO_DIR"
sixgill = "$TOOLTOOL_DIR/sixgill/usr/libexec/sixgill"
sixgill_bin = "$TOOLTOOL_DIR/sixgill/usr/bin"
EOF

        cat > run-analysis.sh <<EOF
#!/bin/sh
if [ \$# -eq 0 ]; then
  set gcTypes
fi
export ANALYSIS_SCRIPTDIR="$ANALYSIS_SRCDIR"
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
        $PYTHON "$ANALYSIS_SRCDIR/analyze.py" --buildcommand="$GECKO_DIR/testing/mozharness/scripts/spidermonkey/build.${build_type}"
    )
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
        for f in *.txt *.lst; do
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

    set +x
    echo "TinderboxPrint: rooting hazards<br/>$NUM_HAZARDS"
    echo "TinderboxPrint: unsafe references to unrooted GC pointers<br/>$NUM_UNSAFE"
    echo "TinderboxPrint: unnecessary roots<br/>$NUM_UNNECESSARY"

    if [ $NUM_HAZARDS -gt 0 ]; then
        echo "TEST-UNEXPECTED-FAIL $NUM_HAZARDS hazards detected" >&2
        echo "TinderboxPrint: documentation<br/><a href='https://wiki.mozilla.org/Javascript:Hazard_Builds'>static rooting hazard analysis failures</a>, visit \"Inspect Task\" link for hazard details"
        exit 1
    fi
    )
}
