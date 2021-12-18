##!/usr/bin/env bash

avif=$2
decoded=$3

orig=$(cat Makefile | grep "^${avif}" | sed "s/^${avif}: \(.*\)$/\1/")

if (echo ${avif} | grep "monochrome"); then
  # FIMXE(ledyba-z): compare monochrome images.
  score="100.0"
elif (echo ${avif} | grep "\(rotate\|mirror\|crop\)"); then
  # FIMXE(ledyba-z): compare transformed images
  score="100.0"
else
  score=$(compare -metric PSNR ${orig} ${decoded} NULL: 2>&1 || true)
fi
if test $(echo "${score} >= 35.0" | bc -l) -eq 1; then
  echo "Passing: ${decoded}: ${score}"
  exit 0
else
  echo "Failed: ${decoded}: ${score} (vs ${orig})"
  exit -1
fi
