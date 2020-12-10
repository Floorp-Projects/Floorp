#!/usr/bin/env bash

set -ue # its like javascript, everything is allowed unless you prevent it.

topdir=$(git rev-parse --show-toplevel)

cd $topdir
# setup: persist the scripts between commits
mkdir -p tmp
cp -r .metrics tmp/
git checkout master
git pull origin master

# create the log of commits
git log --format=oneline --since=2020-01-01 | tac | awk '{print $1}' > tmp/commit-list
cd tmp/.metrics

# do stuff with the commits
for commit in $(cat $topdir/tmp/commit-list)
do
  git checkout $commit
  # python script pulls from env variables, export those
  export total_count=$(find $topdir/rust -iname '*.rs' -type f -exec cat {} + | grep -c -E "(Emit|Parse|ScopeBuild)Error::NotImplemented")
  export current_commit=$commit
  python not_implemented_count.py
  python not_implemented_badge.py
done

cd $topdir
git checkout $ci_branch

# replace this file stuff with whatever it is you want to do to get it to the right place in the
# repo
mv -f tmp/.metrics/count/not-implemented.json .metrics/count/not-implemented.json
mv -f tmp/.metrics/badges/not-implemented.json .metrics/badges/not-implemented.json

# Cleanup: Kill the tmp dir
rm -r tmp

git add .
git commit -m"Add NotImplemented"
