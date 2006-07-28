#!/bin/bash
# set -x

. ../common/unpack.sh 
. ../common/download_mars.sh

build_id="2006050817"
product="Firefox"
release="1.5.0.4"
channel="release"

check_update () {
# called with 4 args - platform, source package, target package, update package
    platform=$1
    source_package=$2
    target_package=$3

    # cleanup
    rm -rf source/*
    rm -rf target/*

    unpack_build $platform source "$source_package" 
    unpack_build $platform target "$target_package" 
    
    #mkdir update
    #cp $update_package update/update.mar
    
    case $platform in
        mac|mac-ppc) 
            platform_dirname="*.app"
            ;;
        win32) 
            platform_dirname="bin"
            ;;
        linux) 
            platform_dirname=`echo $product | tr '[A-Z]' '[a-z]'`
            ;;
    esac

    cd source/$platform_dirname;
    $HOME/bin/updater ../../update 0
    
    cd ../..
    
    diff -r source/$platform_dirname target/$platform_dirname 
    return $?
}

for build_platform in Linux_x86-gcc3 Darwin_Universal-gcc3 WINNT_x86-msvc 
do

  # cleanup
  mkdir -p downloads/
  rm -rf downloads/*

  #for locale in `cat all-locales`
  for locale in en-US
  do
    if [ "Darwin_ppc-gcc3" == $build_platform ]; then
      source_build_platform=mac-ppc
      dirname="mac-ppc"
      filename="$product _version_.dmg"
      shortfilename="`echo $product | tr '[A-Z]' '[a-z]'`-_version_.$locale.$dirname.dmg"
    elif [ "Darwin_Universal-gcc3" == $build_platform ]; then
      source_build_platform=mac
      dirname="mac"
      filename="$product _version_.dmg"
      shortfilename="`echo $product | tr '[A-Z]' '[a-z]'`-_version_.$locale.$dirname.dmg"
    elif [ "Linux_x86-gcc3" == $build_platform ]; then
      source_build_platform=linux
      dirname="linux-i686"
      #dirname="linux"
      filename="`echo $product | tr '[A-Z]' '[a-z]'`-_version_.tar.gz"
      shortfilename="`echo $product | tr '[A-Z]' '[a-z]'`-_version_.$locale.$dirname.tar.gz"
    elif [ "WINNT_x86-msvc" == $build_platform ]; then
      source_build_platform=win32
      dirname="win32"
      filename="$product Setup _version_.exe"
      shortfilename="`echo $product | tr '[A-Z]' '[a-z]'`-_version_.$locale.$dirname.exe"
    fi

    echo "checking $build_platform $locale"

    download_mars "https://aus2.mozilla.org/update/1/$product/$release/$build_id/$build_platform/$locale/$channel/update.xml" partial
    if [ $? != 0 ]; then
      echo "download_mars returned error, skipping.."
      continue
    fi
    source_file=`echo $filename | sed "s/_version_/$release/"`
    target_file=`echo $filename | sed "s/_version_/1.5.0.5/"`
    #target_file=`echo $shortfilename | sed "s/_version_/1.5.0.5/"`
    cp update/partial.mar update/update.mar
    if [ -f "$source_file" ]; then rm "$source_file"; fi
    #PARAMS="--user=user --password=pass"
    build_url="http://stage.mozilla.org/pub/mozilla.org/`echo $product | tr '[A-Z]' '[a-z]'`/releases/$release/$dirname/$locale/$source_file" 
    #build_url="http://people.mozilla.org/~rhelmer/`echo $product | tr '[A-Z]' '[a-z]'`/releases/yahoo/$release/$dirname/$locale/$source_file"
    #build_url="http://people.mozilla.org/~rhelmer/`echo $product | tr '[A-Z]' '[a-z]'`/releases/google/$release/$locale/$source_file" 
    pushd downloads > /dev/null
    if [ -f "$source_file" ]; then rm "$source_file"; fi
    wget -nv $PARAMS "$build_url"
    popd > /dev/null
    if [ $? != 0 ]; then
      echo "FAIL: Could not download source $source_file from $build_url"
      echo "skipping.."
      continue
    fi
    if [ -f "$target_file" ]; then rm "$target_file"; fi
    #build_url="http://stage.mozilla.org/pub/mozilla.org/`echo $product | tr '[A-Z]' '[a-z]'`/nightly/1.5.0.5-candidates/rc4/$target_file" 
    build_url="http://stage.mozilla.org/pub/mozilla.org/`echo $product | tr '[A-Z]' '[a-z]'`/releases/1.5.0.5/$dirname/$locale/$target_file" 
    # XXX hack to test mac-ppc -> mac
    # build_url="http://stage.mozilla.org/pub/mozilla.org/`echo $product | tr '[A-Z]' '[a-z]'`/releases/1.5.0.5/mac/$locale/$target_file" 
    #build_url="http://people.mozilla.org/~rhelmer/`echo $product | tr '[A-Z]' '[a-z]'`/releases/yahoo/testing/1.5.0.5/$dirname/$locale/$target_file" 
    #build_url="http://people.mozilla.org/~rhelmer/`echo $product | tr '[A-Z]' '[a-z]'`/releases/google/testing/1.5.0.5/$dirname/$locale/$target_file" 
    pushd downloads > /dev/null
    if [ -f "$target_file" ]; then rm "$target_file"; fi
    wget -nv $PARAMS "$build_url"
    popd > /dev/null
    if [ $? != 0 ]; then
      echo "FAIL: Could not download target $target_file from $build_url"
      echo "skipping.."
      continue
    fi
    check_update "$source_build_platform" "downloads/$source_file" "downloads/$target_file"

  done
  
done
