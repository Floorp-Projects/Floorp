# Contributing to geckodriver

The geckodriver project welcomes contributions from everyone. There are a
number of ways you can help:

## Issue Reports

When opening new issues or commenting on existing issues please make
sure discussions are related to concrete technical issues with the
geckodriver or Marionette software.

For issue reports to be actionable, it must be clear exactly what the
observed and expected behaviours are, and how to set up the state required
to observe the erroneous behaviour. The most useful thing to provide is a
minimal HTML file which allows the problem to be reproduced, plus a
trace-level log from geckodriver showing the wire-protocol calls used to set
up the problem. Please provide [concise reproducible test
cases](http://sscce.org/) and describe what results you are seeing and what
results you expect. Because of the wide variety of client bindings for
WebDriver, clients scripts and logs are typically not very useful if the
verbose geckodriver logs are available. Issues relating to a specific client
should be filed in the issue tracker of that project.

## Code Contributions

If you're looking for easy bugs, have a look at
[issues labelled E-easy](https://github.com/mozilla/geckodriver/issues?utf8=%E2%9C%93&q=is%3Aopen+is%3Aissue+label%3Aeasy+)
on Github.

This document will guide you through the contribution process.

### Step 1: Fork

Fork the project [on Github](https://github.com/mozilla/geckodriver)
and check out your copy locally.

```text
% git clone git@github.com:username/geckodriver.git
% cd geckodriver
% git remote add upstream git://github.com/mozilla/geckodriver.git
```

### Step 2: Branch

Create a feature branch and start hacking:

```text
% git checkout -b my-feature-branch
```

We practice HEAD-based development, which means all changes are applied
directly on top of master.

### Step 3: Commit

First make sure git knows your name and email address:

```text
% git config --global user.name 'Santa Claus'
% git config --global user.email 'santa@example.com'
```

The first line must be meaningful as it's what people see when they
run `git shortlog` or `git log --oneline`.

### Step 4: Rebase

Use `git rebase` (not `git merge`) to sync your work from time to time.

```text
% git fetch upstream
% git rebase upstream/master
```

### Step 5: Push

```text
% git push my-feature-branch
```

Go to https://github.com/yourusername/geckodriver and press the _Pull
Request_ and fill out the form.

Pull requests are usually reviewed within a few days. Reviews will be done
through [Reviewable](https://reviewable.io/reviews/mozilla/geckodriver)

### Step 6: Integration

When code review is complete, a committer will take your PR and
integrate it on geckodriver's master branch. Because we like to keep a
linear history on the master branch, we will normally squash and rebase
your branch history.

## Communication

geckodriver contributors frequent the `#ateam` channel on
[`irc.mozilla.org`](http://chat.mibbit.com/?server=irc.mozilla.org:#ateam).
