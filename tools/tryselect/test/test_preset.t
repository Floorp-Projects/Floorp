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
  Presets from */mozbuild/try_presets.yml: (glob)
  
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
  Presets from */mozbuild/try_presets.yml: (glob)
  
    bar:
      dry_run: true
      no_artifact: true
      platforms:
      - win32
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
    dry_run: true
    no_artifact: true
    platforms:
    - win32
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

  $ ./mach try fuzzy $testargs --save baz -q "'foo" --rebuild 5
  preset saved, run with: --preset=baz

  $ ./mach try fuzzy $testargs --preset baz
  Commit message:
  Fuzzy query='foo
  
  Pushed via `mach try fuzzy`
  Calculated try_task_config.json:
  {
      "env": {
          "TRY_SELECTOR": "fuzzy"
      },
      "rebuild": 5,
      "tasks": [
          "test/foo-debug",
          "test/foo-opt"
      ],
      "version": 1
  }
  

  $ ./mach try $testargs --preset baz
  Commit message:
  Fuzzy query='foo
  
  Pushed via `mach try fuzzy`
  Calculated try_task_config.json:
  {
      "env": {
          "TRY_SELECTOR": "fuzzy"
      },
      "rebuild": 5,
      "tasks": [
          "test/foo-debug",
          "test/foo-opt"
      ],
      "version": 1
  }
  
 
Queries can be appended to presets

  $ ./mach try fuzzy $testargs --preset baz -q "'build"
  Commit message:
  Fuzzy query='foo&query='build
  
  Pushed via `mach try fuzzy`
  Calculated try_task_config.json:
  {
      "env": {
          "TRY_SELECTOR": "fuzzy"
      },
      "rebuild": 5,
      "tasks": [
          "build-baz",
          "test/foo-debug",
          "test/foo-opt"
      ],
      "version": 1
  }
  

  $ ./mach try $testargs --preset baz -xq "'opt"
  Commit message:
  Fuzzy query='foo&query='opt
  
  Pushed via `mach try fuzzy`
  Calculated try_task_config.json:
  {
      "env": {
          "TRY_SELECTOR": "fuzzy"
      },
      "rebuild": 5,
      "tasks": [
          "test/foo-opt"
      ],
      "version": 1
  }
  

  $ ./mach try fuzzy $testargs --list-presets
  Presets from */mozbuild/try_presets.yml: (glob)
  
    bar:
      dry_run: true
      no_artifact: true
      platforms:
      - win32
      selector: syntax
      tags:
      - bar
      talos:
      - all
      tests:
      - none
    baz:
      dry_run: true
      no_artifact: true
      query:
      - "'foo"
      rebuild: 5
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
    dry_run: true
    no_artifact: true
    platforms:
    - win32
    selector: syntax
    tags:
    - bar
    talos:
    - all
    tests:
    - none
  baz:
    dry_run: true
    no_artifact: true
    query:
    - "'foo"
    rebuild: 5
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

Test gecko-profile argument handling. Add in profiling to a preset.

  $ ./mach try fuzzy $testargs --preset baz --gecko-profile-features=nostacksampling,cpu
  Commit message:
  Fuzzy query='foo
  
  Pushed via `mach try fuzzy`
  Calculated try_task_config.json:
  {
      "env": {
          "TRY_SELECTOR": "fuzzy"
      },
      "gecko-profile": true,
      "gecko-profile-features": "nostacksampling,cpu",
      "rebuild": 5,
      "tasks": [
          "test/foo-debug",
          "test/foo-opt"
      ],
      "version": 1
  }
  
Check whether the gecko-profile flags can be used from a preset, and check
dashes vs underscores (presets save with underscores to match ArgumentParser
settings; everything else uses dashes.)

  $ ./mach try fuzzy $testargs --save profile -q "'foo" --rebuild 5 --gecko-profile-features=nostacksampling,cpu
  preset saved, run with: --preset=profile

  $ ./mach try fuzzy $testargs --preset profile
  Commit message:
  Fuzzy query='foo
  
  Pushed via `mach try fuzzy`
  Calculated try_task_config.json:
  {
      "env": {
          "TRY_SELECTOR": "fuzzy"
      },
      "gecko-profile": true,
      "gecko-profile-features": "nostacksampling,cpu",
      "rebuild": 5,
      "tasks": [
          "test/foo-debug",
          "test/foo-opt"
      ],
      "version": 1
  }
  
  $ EDITOR=cat ./mach try fuzzy $testargs --edit-preset profile
  bar:
    dry_run: true
    no_artifact: true
    platforms:
    - win32
    selector: syntax
    tags:
    - bar
    talos:
    - all
    tests:
    - none
  baz:
    dry_run: true
    no_artifact: true
    query:
    - "'foo"
    rebuild: 5
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
  profile:
    dry_run: true
    gecko_profile_features: nostacksampling,cpu
    no_artifact: true
    query:
    - "'foo"
    rebuild: 5
    selector: fuzzy

  $ rm $MOZBUILD_STATE_PATH/try_presets.yml
