#!/bin/sh

pip install awscli

# Necessary for dolhin kernel building
yum install -y bc

# Remove ourselves
rm -f $0
