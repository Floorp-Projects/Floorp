#!/bin/bash

NON_UNIFIED_CFG=$GECKO_PATH/build/non-unified-compat

while IFS= read -r directory
do
  echo "Running non-unified compile test for: $directory"
  ./mach static-analysis check-syntax $directory
  if [ $? -ne 0 ]; then
    # If failure do not continue with the rest of the directories
    exit 1
  fi
done < $NON_UNIFIED_CFG
