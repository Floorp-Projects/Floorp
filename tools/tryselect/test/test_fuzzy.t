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
  
  Commit message:
  Fuzzy with query: 'foo
  
  Pushed via `mach try fuzzy`
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
  
  Commit message:
  Fuzzy with query: 'bar
  
  Pushed via `mach try fuzzy`

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
  
  Commit message:
  Fuzzy with query: 'foo
  
  Pushed via `mach try fuzzy`
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
  
  Commit message:
  Fuzzy with query: 'foo
  
  Pushed via `mach try fuzzy`
