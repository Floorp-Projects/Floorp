#! /bin/bash

set -e

# checkout the script
source $(dirname $0)/checkout-script.sh

# Check that we have a taskid to checkout
if [ -z ${ARTIFACT_TASKID} ]; then
    echo "Please set ARTIFACT_TASKID. Exiting"
    exit 0
fi

# grab the symbols from an arbitrary task
symbol_url=https://queue.taskcluster.net/v1/task/${ARTIFACT_TASKID}/artifacts/public/build/target.crashreporter-symbols-full.zip
wget ${symbol_url}

# run
symbol_zip=$(basename ${symbol_url})
script_name=$(basename ${SCRIPT_PATH})
python -u ${script_name} ${symbol_zip}
