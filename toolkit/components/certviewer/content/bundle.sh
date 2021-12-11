#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

# Helper script to bundle the given library and amend it to be loadable in more
# contexts.

WHICH="${1}"

# Run the browserify command
./node_modules/browserify/bin/cmd.js "$WHICH".js --standalone "$WHICH" -o ./vendor/"$WHICH"_bundle.js

# Amend 'this' in the first line to 'globalThis'
sed -e '1s/{g=this}/{g=globalThis}/' -i "" ./vendor/"$WHICH"_bundle.js

# Append code to export the library
echo "var $WHICH = globalThis.$WHICH;" >> ./vendor/"$WHICH"_bundle.js
echo "var EXPORTED_SYMBOLS = [\"$WHICH\"];" >> ./vendor/"$WHICH"_bundle.js
