export topsrcdir=$TESTDIR/../../../
export MOZBUILD_STATE_PATH=$TMP/mozbuild

export MACHRC=$TMP/machrc
cat > $MACHRC << EOF
[try]
default=syntax
EOF

calculate_hash='import hashlib, os, sys
print hashlib.sha256(os.path.abspath(sys.argv[1])).hexdigest()'
roothash=$(python -c "$calculate_hash" "$topsrcdir")
cachedir=$MOZBUILD_STATE_PATH/cache/$roothash/taskgraph
mkdir -p $cachedir

cat > $cachedir/target_task_set << EOF
test/foo-opt
test/foo-debug
build-baz
EOF

cat > $cachedir/full_task_set << EOF
test/foo-opt
test/foo-debug
test/bar-opt
test/bar-debug
build-baz
EOF

# set mtime to the future so we don't re-generate tasks
find $cachedir -type f -exec touch -d "next day" {} +

export testargs="--no-push --no-artifact"
