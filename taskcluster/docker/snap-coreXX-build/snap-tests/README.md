Debugging tests
================

You can use the `TEST_FILTER` environment variable, e.g., `TEST_FILTER=xxx`
will filter test named `test_xxx`.

Setting `TEST_GECKODRIVER_TRACE` to any value will make Selenium dump a trace
log for debugging.

You can control running headless or not with `TEST_NO_HEADLESS`. Currently,
the copy/paste image test required NOT to run headless.

More useful for local repro, you can set `TEST_NO_QUIT` if you need to keep
inspecting the browser at the end of a test.

Data URL containing the diff screenshot will be dumped to stdout/stderr when
`TEST_DUMP_DIFF` is set in the environment.

Updating reference screenshots
==============================
 - `./mach try fuzzy --push-to-lando --full --env TEST_COLLECT_REFERENCE=1 -q "'snap-upstream-test"`
 - note the successfull task id you want to source
 - you need curl and jq installed
 - ./update-references.sh <TASK_ID>
