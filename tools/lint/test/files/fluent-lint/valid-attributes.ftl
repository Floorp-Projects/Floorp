# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

### Test for valid attributes.

message-with-known-attribute =
    .label = Foo

# String has a known label but with different case, not a warning
message-with-known-attribute-case =
    .Label = Foo

# Warning: unknown attribute
message-with-unknown-attribute =
    .extralabel = Foo

# NO warning: unknown attribute, but commented
# .extralabel is known
message-with-unknown-attribute-commented =
    .extralabel = Foo
