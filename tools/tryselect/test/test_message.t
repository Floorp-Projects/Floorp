  $ . $TESTDIR/setup.sh
  $ cd $topsrcdir

Test custom commit messages with fuzzy selector

  $ ./mach try fuzzy $testargs -q foo --message "Foobar"
  Commit message:
  Foobar
  
  Fuzzy query=foo
  
  mach try command: `./mach try fuzzy --no-push --no-artifact -q foo --message Foobar`
  
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
  
  $ ./mach try fuzzy $testargs -q foo -m "Foobar: {msg}"
  Commit message:
  Foobar: Fuzzy query=foo
  
  mach try command: `./mach try fuzzy --no-push --no-artifact -q foo -m "Foobar: {msg}"`
  
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
  
  $ unset EDITOR
  $ ./mach try fuzzy $testargs -q foo -m > /dev/null 2>&1
  [2]


Test custom commit messages with syntax selector

  $ ./mach try syntax $testargs -p linux -u mochitests --message "Foobar"
  Commit message:
  Foobar
  
  try: -b do -p linux -u mochitests
  
  mach try command: `./mach try syntax --no-push --no-artifact -p linux -u mochitests --message Foobar`
  
  Pushed via `mach try syntax`
  $ ./mach try syntax $testargs -p linux -u mochitests -m "Foobar: {msg}"
  Commit message:
  Foobar: try: -b do -p linux -u mochitests
  
  mach try command: `./mach try syntax --no-push --no-artifact -p linux -u mochitests -m "Foobar: {msg}"`
  
  Pushed via `mach try syntax`
  $ unset EDITOR
  $ ./mach try syntax $testargs -p linux -u mochitests -m > /dev/null 2>&1
  [2]
