download_mars () {
    update_url="$1"

    echo "Using  $update_url"
    curl -s $update_url > update.xml

    mkdir -p update/
    for patch_type in partial complete
      do
      line=`fgrep "patch type=\"$patch_type" update.xml`
      grep_rv=$?

      if [ 0 -ne $grep_rv ]; then
	  echo "FAIL: no $patch_type update found"
	  continue
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
	  continue
      fi
      
      if [ $actual_hash != $hashValue ]; then
	  echo "FAIL: $patch_type wrong hash"
	  echo "FAIL: update.xml hash: $hashValue"
	  echo "FAIL: actual hash: $actual_hash"
	  continue
      fi
      
#      ln -s $mar_location $patch_type.mar

    done
}
