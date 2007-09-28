#!/usr/local/bin/bash -e 
# -*- Mode: Shell-script; tab-width: 4; indent-tabs-mode: nil; -*-

for var in `echo ${!TEST_*}`; do 
	echo ${var}=${!var} >> $TEST_LOG 
done
