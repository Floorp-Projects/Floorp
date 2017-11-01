  $ . $TESTDIR/setup.sh
  $ cd $topsrcdir

Test empty selector

  $ ./mach try empty --no-push
  Commit message:
  No try selector specified, use "Add New Jobs" to select tasks.
  
  Pushed via `mach try empty`
  Calculated try_task_config.json:
  {
    "tasks":[]
  }
  
  $ ./mach try empty --no-push --closed-tree
  Commit message:
  No try selector specified, use "Add New Jobs" to select tasks. ON A CLOSED TREE
  
  Pushed via `mach try empty`
  Calculated try_task_config.json:
  {
    "tasks":[]
  }
  
  $ ./mach try empty --no-push --closed-tree -m "foo {msg} bar"
  Commit message:
  foo No try selector specified, use "Add New Jobs" to select tasks. bar ON A CLOSED TREE
  
  Pushed via `mach try empty`
  Calculated try_task_config.json:
  {
    "tasks":[]
  }
  
