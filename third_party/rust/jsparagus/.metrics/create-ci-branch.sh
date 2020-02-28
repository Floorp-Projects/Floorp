#!/usr/bin/env bash

set -ue # its like javascript, everything is allowed unless you prevent it.
shopt -s extglob

# export the ci_branch we will be using in all shell scripts
export ci_branch=ci_results

topdir=$(git rev-parse --show-toplevel)

cd $topdir

if [ `git branch --list $ci_branch` ]
then
  echo "Branch exists" #We don't need to do anything
else
  git checkout -b $ci_branch

  # clear out the repostory
  git rm -r !(.metrics|.git|tmp)
  git rm -r .github

  cp .metrics/generated_README.md README.md
  mkdir .metrics/badges
  mkdir .metrics/count

  git add .
  git commit -m"Initial commit for results branch"

  # scripts needed to populated. Should be self contained with cleanup of extra files
  cd .metrics && ./populate_not_implemented.sh
  cd $topdir
  cd .metrics && ./populate_fuzzbug.sh

  cd $topdir
  git add .
  git commit -m"Inital run of Populate scripts"
fi
