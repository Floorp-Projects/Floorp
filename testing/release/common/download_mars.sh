download_mars () {
    update_url="$1"
    only="$2"

    echo "Using  $update_url"
    curl -s $update_url > update.xml

    mkdir -p update/
    if [ -z $only ]; then
      only="partial complete"
    fi
    for patch_type in $only
      do
      line=`fgrep "patch type=\"$patch_type" update.xml`
      grep_rv=$?

      if [ 0 -ne $grep_rv ]; then
	      echo "FAIL: no $patch_type update found for $update_url" > /dev/stderr
	      return 1
      fi

      command=`echo $line | sed -e 's/^.*<patch //' -e 's:/>.*$::' -e 's:\&amp;:\&:g'`
      eval "export $command"

      wget -nv -O update/$patch_type.mar $URL 2>&1 
      if [ "$?" != 0 ]; then
        echo "Could not download $patch_type!"
        echo "from: $URL"
      fi
      actual_size=`perl -e "printf \"%d\n\", (stat(\"update/$patch_type.mar\"))[7]"`
      actual_hash=`openssl dgst -$hashFunction update/$patch_type.mar | sed -e 's/^.*= //'`
      
      if [ $actual_size != $size ]; then
	      echo "FAIL: $patch_type from $update_url wrong size" > /dev/stderr
	      echo "FAIL: update.xml size: $size" > /dev/stderr
	      echo "FAIL: actual size: $actual_size" > /dev/stderr
	      return 1
      fi
      
      if [ $actual_hash != $hashValue ]; then
	      echo "FAIL: $patch_type from $update_url wrong hash" > /dev/stderr
	      echo "FAIL: update.xml hash: $hashValue" > /dev/stderr
	      echo "FAIL: actual hash: $actual_hash" > /dev/stderr
	      return 1
      fi

    done
}
