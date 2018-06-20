#!/bin/bash

echo -e "box2d-loadtime - \c"
$1 run.js -- box2d.js 1 
echo -e "box2d-throughput - \c"
$1 run.js -- box2d.js 3 
echo -e "bullet-loadtime - \c"
$1 run.js -- bullet.js 1 
echo -e "bullet-throughput - \c"
$1 run.js -- bullet.js 3 
echo -e "lua_binarytrees-loadtime - \c"
$1 run.js -- lua_binarytrees.js 1 
echo -e "lua_binarytrees-throughput - \c"
$1 run.js -- lua_binarytrees.js 3 
echo -e "box2d-loadtime - \c"
$1 run.js -- zlib.js 1 
echo -e "box2d-throughput - \c"
$1 run.js -- zlib.js 3 
