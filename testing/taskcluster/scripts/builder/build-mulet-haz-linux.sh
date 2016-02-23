#!/bin/bash -ex

################################### build-mulet-haz-linux.sh ###################################
# Ensure all the scripts in this dir are on the path....
DIRNAME=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
PATH=$DIRNAME:$PATH

. desktop-setup.sh

# Install the sixgill tool
TOOLTOOL_MANIFEST=js/src/devtools/rootAnalysis/build/sixgill.manifest
. install-packages.sh "$GECKO_DIR"

# Build a shell to use to run the analysis
HAZARD_SHELL_OBJDIR=$MOZ_OBJDIR/obj-haz-shell
JS_SRCDIR=$GECKO_DIR/js/src
ANALYSIS_SRCDIR=$JS_SRCDIR/devtools/rootAnalysis

( cd $JS_SRCDIR; autoconf-2.13 )
mkdir -p $HAZARD_SHELL_OBJDIR || true
cd $HAZARD_SHELL_OBJDIR
export CC="$GECKO_DIR/gcc/bin/gcc"
export CXX="$GECKO_DIR/gcc/bin/g++"
$JS_SRCDIR/configure --enable-optimize --disable-debug --enable-ctypes --enable-nspr-build --without-intl-api --with-ccache
make -j4

# configure the analysis
mkdir -p $WORKSPACE/analysis || true
cd $WORKSPACE/analysis
cat > defaults.py <<EOF
js = "$HAZARD_SHELL_OBJDIR/dist/bin/js"
analysis_scriptdir = "$ANALYSIS_SRCDIR"
objdir = "$MOZ_OBJDIR"
source = "$GECKO_DIR"
sixgill = "$GECKO_DIR/sixgill/usr/libexec/sixgill"
sixgill_bin = "$GECKO_DIR/sixgill/usr/bin"
EOF

# run the analysis (includes building the tree)
python $ANALYSIS_SRCDIR/analyze.py --buildcommand=$GECKO_DIR/testing/mozharness/scripts/spidermonkey/build.b2g

### Extract artifacts

# Artifacts folder is outside of the cache.
mkdir -p $HOME/artifacts/ || true

cd $WORKSPACE/analysis
ls -lah

for f in *.txt *.lst; do
    gzip -9 -c "$f" > "$HOME/artifacts/$f.gz"
done

# Check whether the user requested .xdb file upload in the top commit comment

if hg log -l1 --template '{desc}\n' | grep -q 'haz: --upload-xdbs'; then
    for f in *.xdb; do
        bzip2 -c "$f" > "$HOME/artifacts/$f.bz2"
    done
fi

### Check for hazards

if grep 'Function.*has unrooted.*live across GC call' rootingHazards.txt; then
    echo "TEST-UNEXPECTED-FAIL hazards detected" >&2
    exit 1
fi

################################### script end ###################################
