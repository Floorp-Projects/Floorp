#!/bin/bash

download_builds() {
  # cleanup
  mkdir -p downloads/
  rm -rf downloads/*

  source_url="$1"
  target_url="$2"

  if [ -z "$source_url" ] || [ -z "$target_url" ]
  then
    echo "download_builds usage: <source_url> <target_url>"
    exit 1
  fi

  for url in "$source_url" "$target_url"
    do
    source_file=$(basename "$url")
    if [ -f "$source_file" ]; then rm "$source_file"; fi
    cd downloads || exit 
    if [ -f "$source_file" ]; then rm "$source_file"; fi
    cached_download "${source_file}" "${url}"
    status=$?
    if [ $status != 0 ]; then
      echo "TEST-UNEXPECTED-FAIL: Could not download source $source_file from $url"
      echo "skipping.."
      cd ../
      return $status
    fi
    cd ../
  done
}
