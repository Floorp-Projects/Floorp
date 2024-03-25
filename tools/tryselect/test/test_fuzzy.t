  $ . $TESTDIR/setup.sh
  $ cd $topsrcdir

Test fuzzy selector

  $ ./mach try fuzzy $testargs -q "'foo"
  Commit message:
  Fuzzy query='foo
  
  mach try command: `./mach try fuzzy --no-push --no-artifact -q "'foo"`
  
  Pushed via `mach try fuzzy`
  Calculated try_task_config.json:
  {
      "parameters": {
          "optimize_target_tasks": false,
          "try_task_config": {
              "env": {
                  "TRY_SELECTOR": "fuzzy"
              },
              "tasks": [
                  "test/foo-debug",
                  "test/foo-opt"
              ]
          }
      },
      "version": 2
  }
  


  $ ./mach try fuzzy $testargs -q "'bar"
  no tasks selected
  $ ./mach try fuzzy $testargs --full -q "'bar"
  Commit message:
  Fuzzy query='bar
  
  mach try command: `./mach try fuzzy --no-push --no-artifact --full -q "'bar"`
  
  Pushed via `mach try fuzzy`
  Calculated try_task_config.json:
  {
      "parameters": {
          "optimize_target_tasks": false,
          "try_task_config": {
              "env": {
                  "TRY_SELECTOR": "fuzzy"
              },
              "tasks": [
                  "test/bar-debug",
                  "test/bar-opt"
              ]
          }
      },
      "version": 2
  }
  

Test multiple selectors

  $ ./mach try fuzzy $testargs --full -q "'foo" -q "'bar"
  Commit message:
  Fuzzy query='foo&query='bar
  
  mach try command: `./mach try fuzzy --no-push --no-artifact --full -q "'foo" -q "'bar"`
  
  Pushed via `mach try fuzzy`
  Calculated try_task_config.json:
  {
      "parameters": {
          "optimize_target_tasks": false,
          "try_task_config": {
              "env": {
                  "TRY_SELECTOR": "fuzzy"
              },
              "tasks": [
                  "test/bar-debug",
                  "test/bar-opt",
                  "test/foo-debug",
                  "test/foo-opt"
              ]
          }
      },
      "version": 2
  }
  

Test query intersection

  $ ./mach try fuzzy $testargs --and -q "'foo" -q "'opt"
  Commit message:
  Fuzzy query='foo&query='opt
  
  mach try command: `./mach try fuzzy --no-push --no-artifact --and -q "'foo" -q "'opt"`
  
  Pushed via `mach try fuzzy`
  Calculated try_task_config.json:
  {
      "parameters": {
          "optimize_target_tasks": false,
          "try_task_config": {
              "env": {
                  "TRY_SELECTOR": "fuzzy"
              },
              "tasks": [
                  "test/foo-opt"
              ]
          }
      },
      "version": 2
  }
  

Test intersection with preset containing multiple queries

  $ ./mach try fuzzy --save foo -q "'test" -q "'opt"
  preset saved, run with: --preset=foo

  $ ./mach try fuzzy $testargs --preset foo -xq "'test"
  Commit message:
  Fuzzy query='test&query='opt&query='test
  
  mach try command: `./mach try fuzzy --no-push --no-artifact --preset foo -xq "'test"`
  
  Pushed via `mach try fuzzy`
  Calculated try_task_config.json:
  {
      "parameters": {
          "optimize_target_tasks": false,
          "try_task_config": {
              "env": {
                  "TRY_SELECTOR": "fuzzy"
              },
              "tasks": [
                  "test/foo-debug",
                  "test/foo-opt"
              ]
          }
      },
      "version": 2
  }
  
  $ ./mach try $testargs --preset foo -xq "'test"
  Commit message:
  Fuzzy query='test&query='opt&query='test
  
  mach try command: `./mach try --no-push --no-artifact --preset foo -xq "'test"`
  
  Pushed via `mach try fuzzy`
  Calculated try_task_config.json:
  {
      "parameters": {
          "optimize_target_tasks": false,
          "try_task_config": {
              "env": {
                  "TRY_SELECTOR": "fuzzy"
              },
              "tasks": [
                  "test/foo-debug",
                  "test/foo-opt"
              ]
          }
      },
      "version": 2
  }
  

Test exact match

  $ ./mach try fuzzy $testargs --full -q "testfoo | 'testbar"
  Commit message:
  Fuzzy query=testfoo | 'testbar
  
  mach try command: `./mach try fuzzy --no-push --no-artifact --full -q "testfoo | 'testbar"`
  
  Pushed via `mach try fuzzy`
  Calculated try_task_config.json:
  {
      "parameters": {
          "optimize_target_tasks": false,
          "try_task_config": {
              "env": {
                  "TRY_SELECTOR": "fuzzy"
              },
              "tasks": [
                  "test/foo-debug",
                  "test/foo-opt"
              ]
          }
      },
      "version": 2
  }
  
  $ ./mach try fuzzy $testargs --full --exact -q "testfoo | 'testbar"
  Commit message:
  Fuzzy query=testfoo | 'testbar
  
  mach try command: `./mach try fuzzy --no-push --no-artifact --full --exact -q "testfoo | 'testbar"`
  
  Pushed via `mach try fuzzy`
  Calculated try_task_config.json:
  {
      "parameters": {
          "optimize_target_tasks": false,
          "try_task_config": {
              "env": {
                  "TRY_SELECTOR": "fuzzy"
              },
              "tasks": [
                  "test/bar-debug",
                  "test/bar-opt"
              ]
          }
      },
      "version": 2
  }
  

Test task config 

  $ ./mach try fuzzy --no-push --artifact -q "'foo"
  Commit message:
  Fuzzy query='foo
  
  mach try command: `./mach try fuzzy --no-push --artifact -q "'foo"`
  
  Pushed via `mach try fuzzy`
  Calculated try_task_config.json:
  {
      "parameters": {
          "optimize_target_tasks": false,
          "try_task_config": {
              "disable-pgo": true,
              "env": {
                  "TRY_SELECTOR": "fuzzy"
              },
              "tasks": [
                  "test/foo-debug",
                  "test/foo-opt"
              ],
              "use-artifact-builds": true
          }
      },
      "version": 2
  }
  
  $ ./mach try fuzzy $testargs --env FOO=1 --env BAR=baz -q "'foo"
  Commit message:
  Fuzzy query='foo
  
  mach try command: `./mach try fuzzy --no-push --no-artifact --env FOO=1 --env BAR=baz -q "'foo"`
  
  Pushed via `mach try fuzzy`
  Calculated try_task_config.json:
  {
      "parameters": {
          "optimize_target_tasks": false,
          "try_task_config": {
              "env": {
                  "BAR": "baz",
                  "FOO": "1",
                  "TRY_SELECTOR": "fuzzy"
              },
              "tasks": [
                  "test/foo-debug",
                  "test/foo-opt"
              ]
          }
      },
      "version": 2
  }
  
