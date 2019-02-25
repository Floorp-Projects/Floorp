  $ . $TESTDIR/setup.sh
  $ cd $topsrcdir

Test preset with no subcommand

  $ ./mach try $testargs --save foo -b do -p linux -u mochitests -t none --tag foo
  preset saved, run with: --preset=foo

  $ ./mach try $testargs --preset foo
  Commit message:
  try: -b do -p linux -u mochitests -t none --tag foo
  
  Pushed via `mach try syntax`

  $ ./mach try syntax $testargs --preset foo
  Commit message:
  try: -b do -p linux -u mochitests -t none --tag foo
  
  Pushed via `mach try syntax`

  $ ./mach try $testargs --list-presets
  foo:
    no_artifact: true
    platforms:
    - linux
    selector: syntax
    tags:
    - foo
    talos:
    - none
    tests:
    - mochitests
  
  $ unset EDITOR
  $ ./mach try $testargs --edit-presets
  error: must set the $EDITOR environment variable to use --edit-presets
  $ export EDITOR=cat
  $ ./mach try $testargs --edit-presets
  foo:
    no_artifact: true
    platforms:
    - linux
    selector: syntax
    tags:
    - foo
    talos:
    - none
    tests:
    - mochitests

Test preset with syntax subcommand

  $ ./mach try syntax $testargs --save bar -b do -p win32 -u none -t all --tag bar
  preset saved, run with: --preset=bar

  $ ./mach try syntax $testargs --preset bar
  Commit message:
  try: -b do -p win32 -u none -t all --tag bar
  
  Pushed via `mach try syntax`

  $ ./mach try $testargs --preset bar
  Commit message:
  try: -b do -p win32 -u none -t all --tag bar
  
  Pushed via `mach try syntax`

  $ ./mach try syntax $testargs --list-presets
  bar:
    no_artifact: true
    platforms:
    - win32
    push: false
    selector: syntax
    tags:
    - bar
    talos:
    - all
    tests:
    - none
  foo:
    no_artifact: true
    platforms:
    - linux
    selector: syntax
    tags:
    - foo
    talos:
    - none
    tests:
    - mochitests
  
  $ ./mach try syntax $testargs --edit-presets
  bar:
    no_artifact: true
    platforms:
    - win32
    push: false
    selector: syntax
    tags:
    - bar
    talos:
    - all
    tests:
    - none
  foo:
    no_artifact: true
    platforms:
    - linux
    selector: syntax
    tags:
    - foo
    talos:
    - none
    tests:
    - mochitests

Test preset with fuzzy subcommand

  $ ./mach try fuzzy $testargs --save baz -q "'baz"
  preset saved, run with: --preset=baz
  $ ./mach try fuzzy $testargs --preset baz
  Commit message:
  Fuzzy query='baz
  
  Pushed via `mach try fuzzy`
  Calculated try_task_config.json:
  {
      "tasks": [
          "build-baz"
      ],
      "templates": {
          "env": {
              "TRY_SELECTOR": "fuzzy"
          }
      },
      "version": 1
  }
  

  $ ./mach try fuzzy $testargs --preset baz -q "'foo"
  Commit message:
  Fuzzy query='baz&query='foo
  
  Pushed via `mach try fuzzy`
  Calculated try_task_config.json:
  {
      "tasks": [
          "build-baz",
          "test/foo-debug",
          "test/foo-opt"
      ],
      "templates": {
          "env": {
              "TRY_SELECTOR": "fuzzy"
          }
      },
      "version": 1
  }
  
  $ ./mach try $testargs --preset baz
  Commit message:
  Fuzzy query='baz
  
  Pushed via `mach try fuzzy`
  Calculated try_task_config.json:
  {
      "tasks": [
          "build-baz"
      ],
      "templates": {
          "env": {
              "TRY_SELECTOR": "fuzzy"
          }
      },
      "version": 1
  }
  
  $ ./mach try fuzzy $testargs --list-presets
  bar:
    no_artifact: true
    platforms:
    - win32
    push: false
    selector: syntax
    tags:
    - bar
    talos:
    - all
    tests:
    - none
  baz:
    no_artifact: true
    push: false
    query:
    - '''baz'
    selector: fuzzy
  foo:
    no_artifact: true
    platforms:
    - linux
    selector: syntax
    tags:
    - foo
    talos:
    - none
    tests:
    - mochitests
  
  $ ./mach try fuzzy $testargs --edit-presets
  bar:
    no_artifact: true
    platforms:
    - win32
    push: false
    selector: syntax
    tags:
    - bar
    talos:
    - all
    tests:
    - none
  baz:
    no_artifact: true
    push: false
    query:
    - '''baz'
    selector: fuzzy
  foo:
    no_artifact: true
    platforms:
    - linux
    selector: syntax
    tags:
    - foo
    talos:
    - none
    tests:
    - mochitests
