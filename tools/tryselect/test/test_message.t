  $ . $TESTDIR/setup.sh
  $ cd $topsrcdir

Test custom commit messages with fuzzy selector

  $ ./mach try fuzzy $testargs -q foo --message "Foobar"
  Commit message:
  Foobar
  
  Fuzzy query=foo
  
  Pushed via `mach try fuzzy`
  Calculated try_task_config.json:
  {
    "tasks":[
      "test/foo-debug",
      "test/foo-opt"
    ]
  }
  
  $ ./mach try fuzzy $testargs -q foo -m "Foobar: {msg}"
  Commit message:
  Foobar: Fuzzy query=foo
  
  Pushed via `mach try fuzzy`
  Calculated try_task_config.json:
  {
    "tasks":[
      "test/foo-debug",
      "test/foo-opt"
    ]
  }
  
  $ unset EDITOR
  $ ./mach try fuzzy $testargs -q foo -m > /dev/null 2>&1
  [2]


Test custom commit messages with syntax selector

  $ ./mach try syntax $testargs -p linux -u mochitests --message "Foobar"
  Commit message:
  Foobar
  
  try: -b do -p linux -u mochitests
  
  Pushed via `mach try syntax`
  $ ./mach try syntax $testargs -p linux -u mochitests -m "Foobar: {msg}"
  Commit message:
  Foobar: try: -b do -p linux -u mochitests
  
  Pushed via `mach try syntax`
  $ unset EDITOR
  $ ./mach try syntax $testargs -p linux -u mochitests -m > /dev/null 2>&1
  [2]
