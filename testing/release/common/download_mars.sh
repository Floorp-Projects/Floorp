download_mars () {
    update_url="$1"
    only="$2"
    test_only="$3"

    echo "Using  $update_url"
    wget --no-check-certificate -q -O update.xml $update_url

    mkdir -p update/
    if [ -z $only ]; then
      only="partial complete"
    fi
    for patch_type in $only
      do
      line=`fgrep "patch type=\"$patch_type" update.xml`
      grep_rv=$?

      if [ 0 -ne $grep_rv ]; then
	      echo "FAIL: no $patch_type update found for $update_url" 
	      return 1
      fi

      command=`echo $line | sed -e 's/^.*<patch //' -e 's:/>.*$::' -e 's:\&amp;:\&:g'`
      eval "export $command"

      if [ "$test_only" == "1" ]
      then
        echo "Testing $URL"
        curl -k -s -I -L $URL
        return
      else
        wget --no-check-certificate -nv -O update/$patch_type.mar $URL 2>&1 
      fi
      if [ "$?" != 0 ]; then
        echo "Could not download $patch_type!"
        echo "from: $URL"
      fi
      actual_size=`perl -e "printf \"%d\n\", (stat(\"update/$patch_type.mar\"))[7]"`
      actual_hash=`openssl dgst -$hashFunction update/$patch_type.mar | sed -e 's/^.*= //'`
      
      if [ $actual_size != $size ]; then
	      echo "FAIL: $patch_type from $update_url wrong size"
	      echo "FAIL: update.xml size: $size"
	      echo "FAIL: actual size: $actual_size"
	      return 1
      fi
      
      if [ $actual_hash != $hashValue ]; then
	      echo "FAIL: $patch_type from $update_url wrong hash"
	      echo "FAIL: update.xml hash: $hashValue"
	      echo "FAIL: actual hash: $actual_hash"
	      return 1
      fi

      cp update/$patch_type.mar update/update.mar

    done
}
