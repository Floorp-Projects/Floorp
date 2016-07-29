#! /bin/bash -e

# This file is a wrapper around docker build with specific concerns around image
# versions and registry deployment... It also attempts to detect any potential
# missing dependencies and warns you about them.

gecko_root="$( cd "$( dirname "${BASH_SOURCE[0]}" )/../.." && pwd )"

usage() {
  echo "Build a docker image (and tag it)"
  echo
  echo "$0 <image-name>"
  echo
  echo "  Images are defined in testing/docker/<image-name>."
  echo "  For more see: $PWD/README.md"
  echo
}

usage_err() {
  echo $1
  echo
  usage
  exit 1
}

build() {
  local image_name=$1
  local folder="$gecko_root/testing/docker/$image_name"
  local folder_reg="$folder/REGISTRY"
  local folder_ver="$folder/VERSION"
  local could_deploy=false

  if [ "$image_name" == "" ];
  then
    usage
    return
  fi

  test -d "$folder" || usage_err "Unknown image: $image_name"

  # Assume that if an image context directory does not contain a VERSION file then
  # it is not suitable for deploying.  Default to using 'latest' as the tag and
  # warn the user at the end.
  if [ ! -f $folder_ver ]; then
    echo "This image does not contain a VERSION file.  Will use 'latest' as the image version"
    local tag="$image_name:latest"
  else
    local version=$(cat $folder_ver)
    test -n "$version" || usage_err "$folder_ver is empty aborting..."

    # Fallback to default registry if one is not in the folder...
    if [ ! -f "$folder_reg" ]; then
      folder_reg=$PWD/REGISTRY
    fi

    local registry=$(cat $folder_reg)
    test -n "$registry" || usage_err "$folder_reg is empty aborting..."

    local tag="$registry/$image_name:$version"
    local could_deploy=true
  fi

  if [ -f $folder/build.sh ]; then
    shift
    $folder/build.sh -t $tag $* || exit 1
  else
    # use --no-cache so that we always get the latest updates from yum
    # and use the latest version of system-setup.sh
    ( cd $folder/.. && docker build --no-cache -t $tag $image_name ) || exit 1
  fi

  echo "Success built $image_name and tagged with $tag"
  if [ "$could_deploy" = true ]; then
    echo "If deploying now you can run 'docker push $tag'"
  else
    echo "*****************************************************************"
    echo "WARNING: No VERSION file was found in the image directory."
    echo "Image has not been prepared for deploying at this time."
    echo "However, the image can be run locally. To prepare to "
    echo "push to a user account on a docker registry, tag the image "
    echo "by running 'docker tag $tag [REGISTRYHOST/][USERNAME/]NAME[:TAG]"
    echo "prior to running 'docker push'."
    echo "*****************************************************************"
  fi
}

build $*
