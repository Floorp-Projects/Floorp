#! /usr/bin/env node
"use strict";

/* eslint-disable no-console */

let args = process.argv.slice(2);

for (let arg of args) {
    console.log(`dep:${arg}`);
}

