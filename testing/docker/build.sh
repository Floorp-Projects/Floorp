#! /bin/bash -e

# This file is a wrapper around docker build with specific concerns around image
# versions and registry deployment... It also attempts to detect any potential
# missing dependencies and warns you about them.

gecko_root="$( cd "$( dirname "${BASH_SOURCE[0]}" )/../.." && pwd )"

build() {
  local image_name=$1
  local tag=$2
  local folder="$gecko_root/testing/docker/$image_name"

  # use --no-cache so that we always get the latest updates from yum
  # and use the latest version of system-setup.sh
  ( cd $folder/.. && docker build --no-cache -t $tag $image_name ) || exit 1

  echo "Success built $image_name and tagged with $tag"
}

build $*
