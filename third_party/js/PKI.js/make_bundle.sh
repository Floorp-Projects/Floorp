#!/bin/sh

patch -p1  < make-esmodule-bundle.patch
npm install
npm run build
