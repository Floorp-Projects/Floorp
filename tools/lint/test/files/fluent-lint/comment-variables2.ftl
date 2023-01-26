# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

## Terms are not checked for variables, no need for comments.

-term-without-comment1 = { $term1 }
-term-without-comment2 = {
    $select2 ->
        [t] Foo { $term2 }
       *[s] Bar
}

# Testing that variable references from terms are not kept around when analyzing
# standard messages (see bug 1812568).
#
# Variables:
# $message1 (String) - Just text
message-with-comment = This string has a { $message1 }

# This comment is not necessary, just making sure it doesn't get carried over to
# the following message which uses the same variable.
#
# Variables:
# $term-message (string) - Text
-term-with-variable = { $term-message }
message-without-comment = This string has a { $term-message }
