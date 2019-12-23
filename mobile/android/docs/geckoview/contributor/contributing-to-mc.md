---
layout: default
title: Mozilla Central Contributor Guide
summary: Guide to setting up as a contributor to mozilla-central.
tags: [GeckoView,Gecko,mozilla,android,WebView,mobile,mozilla-central,bug fix,submit,patch,arcanist,arc,moz-phab,phabricator]
nav_exclude: true
exclude: true
---
## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

# Submitting a patch to Firefox using Git.

This guide will take you through submitting and updating a patch to `mozilla-central` as a git user. You need to already be [set up to use git to contribute to `mozilla-central`](mc-quick-start).

## Performing a bug fix

All of the open bugs for issues in Firefox can be found in [Bugzilla](https://bugzilla.mozilla.org). If you know the component that you wish to contribute to you can use Bugzilla to search for issues in that project. If you are unsure which component you are interested in, you can search the [Good First Bugs](https://bugzilla.mozilla.org/buglist.cgi?quicksearch=good-first-bug) list to find something you want to work on. 

* Once you have your bug, assign it to yourself in Bugzilla.
* Update your local copy of the firefox codebase to match the current version on the servers to ensure you are working with the most up to date code.

```bash
git remote update
```
* Create a new feature branch tracking either Central or Inbound.

```bash
git checkout -b bugxxxxxxx [inbound|central]/default
```
* Work on your bug, checking into git according to your preferred workflow. _Try to ensure that each individual commit compiles and passes all of the tests for your component. This will make it easier to land if you use `moz-phab` to submit (details later in this post)._

It may be helpful to have Mozilla commit access, at least level 1. There are three levels of commit access that give increasing levels of access to the repositories.

Level 1: Try/User access. You will need this level of access commit to the try server. 
Level 2: General access. This will give you full commit access to any mercurial or SVN repository not requiring level 3 access.
Level 3: Core access. You will need this level to commit directly to any of the core repositories (Firefox/Thunderbird/Fennec).

If you wish to apply for commit access, please follow the guide found in the [Mozilla Commit Access Policy](https://www.mozilla.org/en-US/about/governance/policies/commit/access-policy/).

### Submitting a patch that touches C/C++

If your patch makes changes to any C or C++ code and your editor does not have `clang-format` support, you should run the clang-format linter before submitting your patch to ensure that your code is properly formatted.

```bash
mach clang-format -p path/to/file.cpp
```

Note that `./mach bootstrap` will offer to set up a commit hook that will automatically do this for you.

### Submitting to `try` with Level 1 commit access.

If you only have Level 1 access, you will still need to submit your patch through phabricator, but you can test it on the try server first.

* Use `./mach try fuzzy` to select jobs to run and push to try.

### Submitting a patch via Phabricator. 

To commit anything to the repository, you will need to set up Arcanist and Phabricator. If you are using `git-cinnabar` then you will need to use git enabled versions of these tools.

#### Install Arcanist (Github version)

* Ensure PHP is installed
* [Install Arcanist](https://secure.phabricator.com/book/phabricator/article/arcanist_quick_start/) 

#### Set up Phabricator

* In a browser, visit Mozilla's Phabricator instance at https://phabricator.services.mozilla.com/.
* Click "Log In" at the top of the page

  ![alt text]({{ site.url }}/assets/LogInPhab.png "Log in to Phabricator")
* Click the "Log In or Register" button on the next page. This will take you to Bugzilla to log in or register a new account.

  ![alt text]({{ site.url }}/assets/LogInOrRegister.png "Log in or register a Phabiricator account")
* Sign in with your Bugzilla credentials, or create a new account.

  ![alt text]({{ site.url }}/assets/LogInBugzilla.png "Log in with Bugzilla")
* You will be redirected back to Phabricator, where you will have to create a new Phabricator account.
  <Screenshot Needed>
* Fill in/amend any fields on the form and click "Register Account".
  <Screenshot Needed>
* You now have a Phabricator account and can submit and review patches.

#### Using Arcanist to submit a patch

* Ensure you are on the branch where you have commits that you want to submit.

```bash
git checkout "your-branch-with-commits"
```
* Create a differential patch containing your commits

```bash
arc diff
```

* If you have any uncommitted files, Arcanist will ask if you want to commit them.
* If you have any files in the path not added to git Arcanist will ask if you want to ignore them. 
* After formatting your patch, Arcanist will open a nano/emacs file for you to enter the commit details. If you have many individual git commits in your arcanist diff then the first line of the first commit message will become the patch title, and the rest of the commit, plus the messages for the other commits in the patch will form the summary.
* Ensure you have entered the bug number against the `Bug #` field.
* If you know who you want to review your patch, put their Phabricator handle against the `reviewers` field. If in doubt, look to see who filed, or is listed as a mentor on, the bug you are addressing and choose them.
* Close the editor (Ctrl X) to save the patch.
* Arcanist now formats your patch and submits it to Phabricator. It will display the Phabricator link in the output.
* Copy that link and paste it into a browser window to view your patch.

You may have noticed when using Arcanist that it wraps all of your carefully curated Github commits into a single patch. If you have made many commits that are self contained and pass all the tests then you may wish to submit a patch for each commit. This will make it easier to review. The way to do this is via `moz-phab`. `moz-phab` required Arcanist so you do have to have that installed first.

#### Installing `moz-phab`

```bash
pip install MozPhab [--user]
```

#### Submitting a patch using `moz-phab`.

* Ensure you are on the branch where you have commits that you want to submit.

```bash
git checkout your-branch
```
* Check the revision numbers for the commits you want to submit

```bash
git log
```
* Run `moz-phab`. Specifying a start commit will submit all commits from that commit. Specifying an end commit will submit all commits up to that commit. If no positional arguments are provided, the range is determined to be starting with the first non-public, non-obsolete changeset (for Mercurial) and ending with the currently checked-out changeset.

```bash
moz-phab submit [start_rev] [end_rev]
```
* You will receive a Phabricator link for each commit in the set.

### Updating a patch

* Often you will need to make amendments to a patch after it has been submitted to address review comments. To do this, add your commits to the base branch of your fix as normal. 

To submit the update using Arcanist, run `arc diff --update <PhabricatorDifferentialNumber>`. 

For `moz-phab` run in the same way as the initial submission with the same arguments, that is, specifying the full original range of commits. Note that, while inserting and amending commits should work fine, reordering commits is not yet supported, and deleting commits will leave the associated revisions open, which should be abandoned manually
