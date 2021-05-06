
  $ . $TESTDIR/setup.sh
  $ cd $topsrcdir

Test auto selector

  $ ./mach try auto $testargs
  Commit message:
  Tasks automatically selected.
  
  Pushed via `mach try auto`
  Calculated try_task_config.json:
  {
      "parameters": {
          "optimize_strategies": "taskgraph.optimize:tryselect.bugbug_reduced_manifests_config_selection_low",
          "optimize_target_tasks": true,
          "target_tasks_method": "try_auto",
          "test_manifest_loader": "bugbug",
          "try_mode": "try_auto",
          "try_task_config": {}
      },
      "version": 2
  }
  

  $ ./mach try auto $testargs --closed-tree
  Commit message:
  Tasks automatically selected. ON A CLOSED TREE
  
  Pushed via `mach try auto`
  Calculated try_task_config.json:
  {
      "parameters": {
          "optimize_strategies": "taskgraph.optimize:tryselect.bugbug_reduced_manifests_config_selection_low",
          "optimize_target_tasks": true,
          "target_tasks_method": "try_auto",
          "test_manifest_loader": "bugbug",
          "try_mode": "try_auto",
          "try_task_config": {}
      },
      "version": 2
  }
  
  $ ./mach try auto $testargs --closed-tree -m "foo {msg} bar"
  Commit message:
  foo Tasks automatically selected. bar ON A CLOSED TREE
  
  Pushed via `mach try auto`
  Calculated try_task_config.json:
  {
      "parameters": {
          "optimize_strategies": "taskgraph.optimize:tryselect.bugbug_reduced_manifests_config_selection_low",
          "optimize_target_tasks": true,
          "target_tasks_method": "try_auto",
          "test_manifest_loader": "bugbug",
          "try_mode": "try_auto",
          "try_task_config": {}
      },
      "version": 2
  }
  
