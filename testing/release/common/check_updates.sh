check_updates () {
  # called with 4 args - platform, source package, target package, update package
  update_platform=$1
  source_package=$2
  target_package=$3

  # cleanup
  rm -rf source/*
  rm -rf target/*

  unpack_build $update_platform source "$source_package" 
  if [ "$?" != "0" ]; then
    echo "FAILED: cannot unpack_build $update_platform source $source_package" |tee /dev/stderr
    return 1
  fi
  unpack_build $update_platform target "$target_package" 
  if [ "$?" != "0" ]; then
    echo "FAILED: cannot unpack_build $update_platform target $target_package" |tee /dev/stderr
    return 1
  fi
  
  case $update_platform in
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

  if [ -d source/$platform_dirname ]; then
    cd source/$platform_dirname;
    $HOME/bin/updater ../../update 0
    cd ../..
  else
    echo "FAIL: no dir in source/$platform_dirname" |tee /dev/stderr
    return 1
  fi
  
  diff -r source/$platform_dirname target/$platform_dirname 
  return $?
}
