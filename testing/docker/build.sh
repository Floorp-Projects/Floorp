#! /bin/bash -e

# This file is a wrapper around docker build with specific concerns around image
# versions and registry deployment... It also attempts to detect any potential
# missing dependencies and warns you about them.

usage() {
  echo "Build a docker image in the given folder (and tag it)"
  echo
  echo "$0 <folder>"
  echo
  echo "  For more see: $PWD/README.md"
  echo
}

usage_err() {
  echo $1
  echo
  usage
  exit 1
}

find_registry() {
  local reg="$1/REGISTRY"

  if [ -f $reg ];
  then
    echo $folder
    return
  fi
}

build() {
  local folder=$1
  local folder_reg="$1/REGISTRY"
  local folder_ver="$1/VERSION"

  if [ "$folder" == "" ];
  then
    usage
    return
  fi

  test -d "$folder" || usage_err "Unknown folder: $folder"
  test -f "$folder_ver" || usage_err "$folder must contain VERSION file"

  # Fallback to default registry if one is not in the folder...
  if [ ! -f "$folder_reg" ]; then
    folder_reg=$PWD/REGISTRY
  fi

  local registry=$(cat $folder_reg)
  local version=$(cat $folder_ver)

  test -n "$registry" || usage_err "$folder_reg is empty aborting..."
  test -n "$version" || usage_err "$folder_ver is empty aborting..."

  local tag="$registry/$folder:$version"

  if [ -f $folder/build.sh ]; then
    shift
    $folder/build.sh -t $tag $*
  else
    # use --no-cache so that we always get the latest updates from yum
    # and use the latest version of system-setup.sh
    docker build --no-cache -t $tag $folder
  fi

  echo "Success built $folder and tagged with $tag"
  echo "If deploying now you can run 'docker push $tag'"
}

if ! which docker > /dev/null; then
  echo "Docker must be installed read installation instructions at docker.com"
  echo
  usage
  exit 1
fi

# TODO: In the future we should check minimum docker version it does matter.
if ! docker version > /dev/null;
then
  echo "Docker server is unresponsive run 'docker ps' and check that docker is"
  echo "running"
  echo
  usage
  exit 1
fi

build $*
