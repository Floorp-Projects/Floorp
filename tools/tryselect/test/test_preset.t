  $ . $TESTDIR/setup.sh
  $ cd $topsrcdir

Test preset with no subcommand

  $ ./mach try $testargs --save foo -b do -p linux -u mochitests -t none --tag foo
  Commit message:
  try: -b do -p linux -u mochitests -t none --tag foo
  
  Pushed via `mach try syntax`
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
  foo: -b do -p linux -u mochitests -t none --tag foo
  $ unset EDITOR
  $ ./mach try $testargs --edit-presets
  error: must set the $EDITOR environment variable to use --edit-presets
  $ export EDITOR=cat
  $ ./mach try $testargs --edit-presets
  [try]
  foo = -b do -p linux -u mochitests -t none --tag foo
  

Test preset with syntax subcommand

  $ ./mach try syntax $testargs --save bar -b do -p win32 -u none -t all --tag bar
  Commit message:
  try: -b do -p win32 -u none -t all --tag bar
  
  Pushed via `mach try syntax`
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
  foo: -b do -p linux -u mochitests -t none --tag foo
  bar: -b do -p win32 -u none -t all --tag bar
  $ ./mach try syntax $testargs --edit-presets
  [try]
  foo = -b do -p linux -u mochitests -t none --tag foo
  bar = -b do -p win32 -u none -t all --tag bar
  

Test preset with fuzzy subcommand

  $ ./mach try fuzzy $testargs --save baz -q "'baz"
  preset saved, run with: --preset=baz
  Commit message:
  Fuzzy query='baz
  
  Pushed via `mach try fuzzy`
  Calculated try_task_config.json:
  {
    "tasks":[
      "build-baz"
    ]
  }
  
  $ ./mach try fuzzy $testargs --preset baz
  Commit message:
  Fuzzy query='baz
  
  Pushed via `mach try fuzzy`
  Calculated try_task_config.json:
  {
    "tasks":[
      "build-baz"
    ]
  }
  

  $ ./mach try fuzzy $testargs --preset baz -q "'foo"
  Commit message:
  Fuzzy query='foo&query='baz
  
  Pushed via `mach try fuzzy`
  Calculated try_task_config.json:
  {
    "tasks":[
      "build-baz",
      "test/foo-debug",
      "test/foo-opt"
    ]
  }
  
  $ ./mach try $testargs --preset baz
  Commit message:
  Fuzzy query='baz
  
  Pushed via `mach try fuzzy`
  Calculated try_task_config.json:
  {
    "tasks":[
      "build-baz"
    ]
  }
  
  $ ./mach try fuzzy $testargs --list-presets
  baz: 'baz
  $ ./mach try fuzzy $testargs --edit-presets
  [try]
  foo = -b do -p linux -u mochitests -t none --tag foo
  bar = -b do -p win32 -u none -t all --tag bar
  
  [fuzzy]
  baz = 'baz
  
