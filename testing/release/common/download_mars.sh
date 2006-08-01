download_mars () {
    update_url="$1"
    only="$2"

    echo "Using  $update_url"
    curl -s $update_url #> update.xml

    mkdir -p update/
    if [ -z $only ]; then
      only="partial complete"
    fi
    for patch_type in $only
      do
      line=`fgrep "patch type=\"$patch_type" update.xml`
      grep_rv=$?

      if [ 0 -ne $grep_rv ]; then
	      echo "FAIL: no $patch_type update found"
	      return 1
      fi

      command=`echo $line | sed -e 's/^.*<patch //' -e 's:/>.*$::' -e 's:\&amp;:\&:g'`
      eval "export $command"

      wget -nv -O update/$patch_type.mar $URL
      if [ "$?" != 0 ]; then
        echo "Could not download $patch_type!"
        echo "from: $URL"
      fi
      actual_size=`perl -e "printf \"%d\n\", (stat(\"update/$patch_type.mar\"))[7]"`
      actual_hash=`openssl dgst -$hashFunction update/$patch_type.mar | sed -e 's/^.*= //'`
      
      if [ $actual_size != $size ]; then
	      echo "FAIL: $patch_type wrong size"
	      echo "FAIL: update.xml size: $size"
	      echo "FAIL: actual size: $actual_size"
	      return 1
      fi
      
      if [ $actual_hash != $hashValue ]; then
	      echo "FAIL: $patch_type wrong hash"
	      echo "FAIL: update.xml hash: $hashValue"
	      echo "FAIL: actual hash: $actual_hash"
	      return 1
      fi

    done
}
