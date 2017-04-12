#!/bin/bash
##
## List the release each author first contributed to.
##
## Usage: author_first_release.sh [TAGS]
##
## If the TAGS arguments are unspecified, all tags reported by `git tag`
## will be considered.
##
tags=${@:-$(git tag)}
for tag in $tags; do
  git shortlog -n -e -s $tag |
      cut -f2- |
      awk "{print \"${tag#v}\t\"\$0}"
done | sort -k2  | uniq -f2
