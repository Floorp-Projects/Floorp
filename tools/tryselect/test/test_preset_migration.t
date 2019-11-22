  $ . $TESTDIR/setup.sh
  $ cd $topsrcdir

Test migration of syntax preset

  $ rm $MOZBUILD_STATE_PATH/try_presets.yml
  $ cat > $MOZBUILD_STATE_PATH/autotry.ini << EOF
  > [try]
  > foo = -b o -p all -u mochitest -t none --tag bar
  > EOF
  $ ls $MOZBUILD_STATE_PATH/autotry.ini
  */mozbuild/autotry.ini (glob)
  $ ./mach try syntax $testargs --list-presets
  migrating saved presets from '*/mozbuild/autotry.ini' to '*/mozbuild/try_presets.yml' (glob)
  Presets from */mozbuild/try_presets.yml: (glob)
  
    foo:
      builds: o
      platforms:
      - all
      selector: syntax
      tags:
      - bar
      talos:
      - none
      tests:
      - mochitest
    
  $ ./mach try syntax $testargs --preset foo
  Commit message:
  try: -b o -p all -u mochitest -t none --tag bar
  
  Pushed via `mach try syntax`
  $ ls $MOZBUILD_STATE_PATH/autotry.ini
  */mozbuild/autotry.ini': No such file or directory (glob)
  [2]
 Test migration of fuzzy preset

  $ rm $MOZBUILD_STATE_PATH/try_presets.yml
  $ cat > $MOZBUILD_STATE_PATH/autotry.ini << EOF
  > [fuzzy]
  > foo = 'foo | 'bar
  > EOF
  $ ls $MOZBUILD_STATE_PATH/autotry.ini
  */mozbuild/autotry.ini (glob)

  $ ./mach try fuzzy $testargs --preset foo
  migrating saved presets from '*/mozbuild/autotry.ini' to '*/mozbuild/try_presets.yml' (glob)
  Commit message:
  Fuzzy query='foo | 'bar
  
  Pushed via `mach try fuzzy`
  Calculated try_task_config.json:
  {
      "env": {
          "TRY_SELECTOR": "fuzzy"
      },
      "tasks": [
          "test/foo-debug",
          "test/foo-opt"
      ],
      "version": 1
  }
  
  $ ls $MOZBUILD_STATE_PATH/autotry.ini
  */mozbuild/autotry.ini': No such file or directory (glob)
  [2]
  $ ./mach try fuzzy $testargs --list-presets
  Presets from */mozbuild/try_presets.yml: (glob)
  
    foo:
      query:
      - '''foo | ''bar'
      selector: fuzzy
    
 
 Test unknown section prints message

  $ rm $MOZBUILD_STATE_PATH/try_presets.yml
  $ cat > $MOZBUILD_STATE_PATH/autotry.ini << EOF
  > [unknown]
  > foo = bar
  > baz = foo
  > [again]
  > invalid = ?
  > EOF
  $ ./mach try $testargs
  migrating saved presets from '*/mozbuild/autotry.ini' to '*/mozbuild/try_presets.yml' (glob)
  warning: unknown section 'unknown', the following presets were not migrated:
    foo = bar
    baz = foo
  
  warning: unknown section 'again', the following presets were not migrated:
    invalid = ?
  
  Either platforms or jobs must be specified as an argument to autotry.
  [1]
