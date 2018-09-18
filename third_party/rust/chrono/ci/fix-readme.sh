#!/bin/bash

VERSION="$( cargo read-manifest | python -c 'import json, sys; print(json.load(sys.stdin)["version"])')"
LIB="$1"

# Make the Chrono in the header a link to the docs
awk '/^\/\/! # Chrono: / { print "[Chrono][docsrs]:", substr($0, index($0, $4))}' "$LIB"
awk '/^\/\/! # Chrono: / { print "[Chrono][docsrs]:", substr($0, index($0, $4))}' "$LIB" | sed 's/./=/g'
# Add all the badges
echo '
[![Chrono on Travis CI][travis-image]][travis]
[![Chrono on Appveyor][appveyor-image]][appveyor]
[![Chrono on crates.io][cratesio-image]][cratesio]
[![Chrono on docs.rs][docsrs-image]][docsrs]
[![Join the chat at https://gitter.im/chrono-rs/chrono][gitter-image]][gitter]

[travis-image]: https://travis-ci.org/chronotope/chrono.svg?branch=master
[travis]: https://travis-ci.org/chronotope/chrono
[appveyor-image]: https://ci.appveyor.com/api/projects/status/2ia91ofww4w31m2w/branch/master?svg=true
[appveyor]: https://ci.appveyor.com/project/chronotope/chrono
[cratesio-image]: https://img.shields.io/crates/v/chrono.svg
[cratesio]: https://crates.io/crates/chrono
[docsrs-image]: https://docs.rs/chrono/badge.svg
[docsrs]: https://docs.rs/chrono
[gitter-image]: https://badges.gitter.im/chrono-rs/chrono.svg
[gitter]: https://gitter.im/chrono-rs/chrono'

# print the section between the header and the usage
awk '/^\/\/! # Chrono:/,/^\/\/! ## /' "$LIB" | cut -b 5- | grep -v '^#' | \
    sed 's/](\.\//](https:\/\/docs.rs\/chrono\/'$VERSION'\/chrono\//g'
echo
# Replace relative doc links with links to this exact version of docs on
# docs.rs
awk '/^\/\/! ## /,!/^\/\/!/' "$LIB" | cut -b 5- | grep -v '^# ' | \
    sed 's/](\.\//](https:\/\/docs.rs\/chrono\/'$VERSION'\/chrono\//g' \
