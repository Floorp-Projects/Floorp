#!/bin/bash
# Checks automatic preconditions for a release
determine_new_version() {
	grep "version = " Cargo.toml | sed -Ee 's/version = "(.*)"/\1/' | head -1
}

check_notexists_version() {
	# Does the api information start with: '{"errors":'
	[[ $(wget "https://crates.io/api/v1/crates/image/$1" -qO -) == "{\"errors\":"* ]]
}

check_release_description() {
	major=${1%%.*}
	minor_patch=${1#$major.}
	minor=${minor_patch%%.*}
	patch=${minor_patch#$minor.}
	# We just need to find a fitting header line
	grep -Eq "^### Version ${major}.${minor}$" CHANGES.md
}

version="$(determine_new_version)"
check_release_description $version || { echo "Version does not have a release description"; exit 1; }
check_notexists_version $version || { echo "Version $version appears already published"; exit 1; }

