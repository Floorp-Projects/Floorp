Sheriffed intermittent failures
===============================
The Firefox Sheriff team looks at all failures for tasks which are visible by default
in Treeherder (tier 1 and 2) and are part of a push on a sheriffed tree (autoland,
mozilla-central, mozilla-beta, mozilla-release, mozilla-esr trees) and determines if the
failure is a regression or an intermittent failure.  In the case of an intermittent
failure, the sheriffs annotate the failure and the annotation is logged in
[Treeherder](https://treeherder.mozilla.org/intermittent-failures).

In most cases the sheriffs will determine if a new bug is needed or an existing bug
already tracks this kind of failure.  In most cases sheriffs will annotate the failure
using a "Single Tracking Bug", in other cases there will be specific failures that are
tracked seperately.

Single tracking bugs
--------------------
Single tracking bugs are used to track test failures seen in CI.  These are tracked
at the test case level (typically path/filename) instead of the error message.
We have found that many times we have >1 bug tracking failures on a test case,
but none of the bugs are frequent enough to get attention of the test owners.
In addition, when a developer is looking into fixing an intermittent failure, they
are debugging the file, and it is great to see ALL the related failures in one place.

There are 2 ways to get detailed information about test failures:
1. [Treeherder](https://treeherder.mozilla.org/intermittent-failures), when viewing a specific issue,
there is a table, on the far right side of the table is the column titled `Log`.
If you click the text box underneath that, a drop down of all failure types will be populated,
selecting a failure will filter on that failure to see logs, etc.
2. from `mach test-info`, one can view a breakdown of all failures and where they occur
(example: `./mach test-info failure-report --bugid 1781668`)


The workflow of single tracking bugs is as follows:
 - Sheriffs find new failures in CI and create new bugs.  If Treeherder can find the path,
 Treeherder will recommend a `single tracking bug` and strip out the error message.
 - Sheriffs will annotate existing bugs if the failure to annotate has a test path and in
 the list of bug suggestions there is a `single tracking bug`.  Treeherder will offer
 that up as the choice as long as the test paths match (and other criteria like crashes,
 assertions, leaks, etc. are met)
 - Sheriffs will needinfo triage owner if enough failures occur
 (currently 30 failures a week, etc.)
 - Developers will be able to investigate the set of failures, any specific bugs they are
 fixing (for some or all of the conditions).  It is best practice to use the
 `single tracking bug` as a META bug and file a new bug blocking the META bug with the specific fix.
