#!/bin/bash

base=$(dirname $0)

# Determine path to build target.
obj_dir=$(cat $base/../../dist/latest 2>/dev/null)

# Run tests.
OBJDIR=$obj_dir $base/all.sh
