download_builds() {
  # cleanup
  mkdir -p downloads/
  rm -rf downloads/*

  if [ "Darwin_ppc-gcc3" == $platform ]; then
    source_platform=mac-ppc
    dirname="mac-ppc"
    filename="$product _version_.dmg"
    shortfilename="`echo $product | tr '[A-Z]' '[a-z]'`-_version_.$locale.$dirname.dmg"
  elif [ "Darwin_Universal-gcc3" == $platform ]; then
    source_platform=mac
    dirname="mac"
    filename="$product _version_.dmg"
    shortfilename="`echo $product | tr '[A-Z]' '[a-z]'`-_version_.$locale.$dirname.dmg"
  elif [ "Linux_x86-gcc3" == $platform ]; then
    source_platform=linux
    dirname="linux-i686"
    #dirname="linux"
    filename="`echo $product | tr '[A-Z]' '[a-z]'`-_version_.tar.gz"
    shortfilename="`echo $product | tr '[A-Z]' '[a-z]'`-_version_.$locale.$dirname.tar.gz"
  elif [ "WINNT_x86-msvc" == $platform ]; then
    source_platform=win32
    dirname="win32"
    filename="$product Setup _version_.exe"
    shortfilename="`echo $product | tr '[A-Z]' '[a-z]'`-_version_.$locale.$dirname.exe"
  fi

  echo "checking $platform $locale"

  source_file=`echo $filename | sed "s/_version_/$release/"`
  target_file=`echo $filename | sed "s/_version_/$latest/"`
  cp update/partial.mar update/update.mar
  if [ -f "$source_file" ]; then rm "$source_file"; fi
  #PARAMS="--user=user --password=pass"
  build_url="http://stage.mozilla.org/pub/mozilla.org/`echo $product | tr '[A-Z]' '[a-z]'`/releases/$release/$dirname/$locale/$source_file" 
  pushd downloads > /dev/null
  if [ -f "$source_file" ]; then rm "$source_file"; fi
  wget -nv $PARAMS "$build_url" 2>&1
  if [ $? != 0 ]; then
    echo "FAIL: Could not download source $source_file from $build_url" |tee /dev/stderr
    echo "skipping.."
    continue
    popd > /dev/null
  fi
  popd > /dev/null
  if [ -f "$target_file" ]; then rm "$target_file"; fi
  build_url="http://stage.mozilla.org/pub/mozilla.org/`echo $product | tr '[A-Z]' '[a-z]'`/releases/$latest/$dirname/$locale/$target_file" 
  pushd downloads > /dev/null
  if [ -f "$target_file" ]; then rm "$target_file"; fi
  wget -nv $PARAMS "$build_url" 2>&1
  if [ $? != 0 ]; then
    echo "FAIL: Could not download target $target_file from $build_url" |tee /dev/stderr
    echo "skipping.."
    continue
    popd > /dev/null
  fi
  popd > /dev/null
}
