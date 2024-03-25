  $ . $TESTDIR/setup.sh
  $ cd $topsrcdir

Test empty selector

  $ ./mach try empty --no-push
  Commit message:
  No try selector specified, use "Add New Jobs" to select tasks.
  
  mach try command: `./mach try empty --no-push`
  
  Pushed via `mach try empty`
  Calculated try_task_config.json:
  {
      "parameters": {
          "optimize_target_tasks": false,
          "try_task_config": {
              "env": {
                  "TRY_SELECTOR": "empty"
              },
              "tasks": []
          }
      },
      "version": 2
  }
  
  $ ./mach try empty --no-push --closed-tree
  Commit message:
  No try selector specified, use "Add New Jobs" to select tasks. ON A CLOSED TREE
  
  mach try command: `./mach try empty --no-push --closed-tree`
  
  Pushed via `mach try empty`
  Calculated try_task_config.json:
  {
      "parameters": {
          "optimize_target_tasks": false,
          "try_task_config": {
              "env": {
                  "TRY_SELECTOR": "empty"
              },
              "tasks": []
          }
      },
      "version": 2
  }
  
  $ ./mach try empty --no-push --closed-tree -m "foo {msg} bar"
  Commit message:
  foo No try selector specified, use "Add New Jobs" to select tasks. bar ON A CLOSED TREE
  
  mach try command: `./mach try empty --no-push --closed-tree -m "foo {msg} bar"`
  
  Pushed via `mach try empty`
  Calculated try_task_config.json:
  {
      "parameters": {
          "optimize_target_tasks": false,
          "try_task_config": {
              "env": {
                  "TRY_SELECTOR": "empty"
              },
              "tasks": []
          }
      },
      "version": 2
  }
  
