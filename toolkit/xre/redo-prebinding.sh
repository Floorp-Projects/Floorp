#!/bin/sh
BINARY_DIR=`pwd|sed 's/ /\\\ /g'`
/bin/sh -c "update_prebinding -files $BINARY_DIR/*"
