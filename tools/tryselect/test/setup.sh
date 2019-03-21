export topsrcdir=$TESTDIR/../../../
export MOZBUILD_STATE_PATH=$TMP/mozbuild
export MACH_TRY_PRESET_PATHS=$MOZBUILD_STATE_PATH/try_presets.yml

export MACHRC=$TMP/machrc
cat > $MACHRC << EOF
[try]
default=syntax
EOF

cmd="$topsrcdir/mach python -c 'from mozboot.util import get_state_dir; print(get_state_dir(srcdir=True))'"
# First run local state dir generation so it doesn't affect test output.
eval $cmd > /dev/null 2>&1
# Now run it again to get the actual directory.
cachedir=$(eval $cmd)/cache/taskgraph
mkdir -p $cachedir

cat > $cachedir/target_task_set << EOF
{
  "test/foo-opt": {
    "kind": "test",
    "label": "test/foo-opt",
    "attributes": {},
    "task": {},
    "optimization": {},
    "dependencies": {}
  },
  "test/foo-debug": {
    "kind": "test",
    "label": "test/foo-debug",
    "attributes": {},
    "task": {},
    "optimization": {},
    "dependencies": {}
  },
  "build-baz": {
    "kind": "build",
    "label": "build-baz",
    "attributes": {},
    "task": {},
    "optimization": {},
    "dependencies": {}
  }
}
EOF

cat > $cachedir/full_task_set << EOF
{
  "test/foo-opt": {
    "kind": "test",
    "label": "test/foo-opt",
    "attributes": {},
    "task": {},
    "optimization": {},
    "dependencies": {}
  },
  "test/foo-debug": {
    "kind": "test",
    "label": "test/foo-debug",
    "attributes": {},
    "task": {},
    "optimization": {},
    "dependencies": {}
  },
  "test/bar-opt": {
    "kind": "test",
    "label": "test/bar-opt",
    "attributes": {},
    "task": {},
    "optimization": {},
    "dependencies": {}
  },
  "test/bar-debug": {
    "kind": "test",
    "label": "test/bar-debug",
    "attributes": {},
    "task": {},
    "optimization": {},
    "dependencies": {}
  },
  "build-baz": {
    "kind": "build",
    "label": "build-baz",
    "attributes": {},
    "task": {},
    "optimization": {},
    "dependencies": {}
  }
}
EOF

# set mtime to the future so we don't re-generate tasks
find $cachedir -type f -exec touch -d "next day" {} +

export testargs="--no-push --no-artifact"
