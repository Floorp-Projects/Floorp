#!/bin/sh

set -e

for i in *.c; do
	mv "$i" "${i}pp"
done