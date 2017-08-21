  $ . $TESTDIR/setup.sh
  $ cd $topsrcdir

Test fuzzy selector

  $ ./mach try fuzzy $testargs -q "'foo"
  Calculated try selector:
  {
    "tasks":[
      "test/foo-debug",
      "test/foo-opt"
    ]
  }
  $ ./mach try fuzzy $testargs -q "'bar"
  no tasks selected
  $ ./mach try fuzzy $testargs --full -q "'bar"
  Calculated try selector:
  {
    "tasks":[
      "test/bar-debug",
      "test/bar-opt"
    ]
  }
