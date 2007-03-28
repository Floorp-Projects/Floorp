download_builds() {
  # cleanup
  mkdir -p downloads/
  rm -rf downloads/*

  source_url="$1"
  target_url="$2"

  if [ -z "$source_url" ] || [ -z "$target_url" ]
  then
    "download_builds usage: <source_url> <target_url>"
    exit 1
  fi

  for url in "$source_url" "$target_url"
    do
    source_file=`basename "$url"`
    if [ -f "$source_file" ]; then rm "$source_file"; fi
    #PARAMS="--user=user --password=pass"
    cd downloads 
    if [ -f "$source_file" ]; then rm "$source_file"; fi
    wget --no-check-certificate -nv $PARAMS "$url" 2>&1
    if [ $? != 0 ]; then
      echo "FAIL: Could not download source $source_file from $url"
      echo "skipping.."
      cd ../
      continue
    fi
    cd ../
  done
}
