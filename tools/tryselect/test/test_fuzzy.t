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

Test templates

  $ ./mach try fuzzy --no-push --artifact -q "'foo"
  Calculated try selector:
  {
    "templates":{
      "artifact":{
        "enabled":"1"
      }
    },
    "tasks":[
      "test/foo-debug",
      "test/foo-opt"
    ]
  }
  $ ./mach try fuzzy $testargs --env FOO=1 --env BAR=baz -q "'foo"
  Calculated try selector:
  {
    "templates":{
      "env":{
        "FOO":"1",
        "BAR":"baz"
      }
    },
    "tasks":[
      "test/foo-debug",
      "test/foo-opt"
    ]
  }
