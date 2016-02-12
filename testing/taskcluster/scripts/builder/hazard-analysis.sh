#!/bin/bash -ex

HAZARD_SHELL_OBJDIR=$MOZ_OBJDIR/obj-haz-shell
JS_SRCDIR=$GECKO_DIR/js/src
ANALYSIS_SRCDIR=$JS_SRCDIR/devtools/rootAnalysis

# Install the sixgill tool
TOOLTOOL_MANIFEST=js/src/devtools/rootAnalysis/build/sixgill.manifest
. install-packages.sh "$GECKO_DIR"

export CC="$GECKO_DIR/gcc/bin/gcc"
export CXX="$GECKO_DIR/gcc/bin/g++"

function build_js_shell () {
    ( cd $JS_SRCDIR; autoconf-2.13 )
    mkdir -p $HAZARD_SHELL_OBJDIR || true
    cd $HAZARD_SHELL_OBJDIR
    $JS_SRCDIR/configure --enable-optimize --disable-debug --enable-ctypes --enable-nspr-build --without-intl-api --with-ccache
    make -j4
}

function configure_analysis () {
    local analysis_dir
    analysis_dir="$1"

    mkdir -p "$analysis_dir" || true
    (
        cd "$analysis_dir"
        cat > defaults.py <<EOF
js = "$HAZARD_SHELL_OBJDIR/dist/bin/js"
analysis_scriptdir = "$ANALYSIS_SRCDIR"
objdir = "$MOZ_OBJDIR"
source = "$GECKO_DIR"
sixgill = "$GECKO_DIR/sixgill/usr/libexec/sixgill"
sixgill_bin = "$GECKO_DIR/sixgill/usr/bin"
EOF
    )
}

function run_analysis () {
    local analysis_dir
    analysis_dir="$1"
    local build_type
    build_type="$2"

    (
        cd "$analysis_dir"
        python "$ANALYSIS_SRCDIR/analyze.py" --buildcommand="$GECKO_DIR/testing/mozharness/scripts/spidermonkey/build.${build_type}"
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

        for f in *.txt *.lst; do
            gzip -9 -c "$f" > "${artifacts}/$f.gz"
        done

        # Check whether the user requested .xdb file upload in the top commit comment

        if hg --cwd "$GECKO_DIR" log -l1 --template '{desc}\n' | grep -q 'haz: --upload-xdbs'; then
            for f in *.xdb; do
                bzip2 -c "$f" > "${artifacts}/$f.bz2"
            done
        fi
    )
}

function check_hazards () {
    if grep 'Function.*has unrooted.*live across GC call' "$1"/rootingHazards.txt; then
        echo "TEST-UNEXPECTED-FAIL hazards detected" >&2
        exit 1
    fi
}
