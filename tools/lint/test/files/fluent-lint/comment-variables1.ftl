# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

### Test group comments.
### $var doesn't count towards commented placeables

message-without-comment = This string has a { $var }

## Variables:
## $foo-group (String): group level comment

# Variables:
# $foo1 (String): just text
message-with-comment = This string has a { $foo1 }

message-with-group-comment = This string has a { $foo-group }

select-without-comment1 = {
    $select1 ->
        [t] Foo
       *[s] Bar
}

select-without-comment2 = {
    $select2 ->
        [t] Foo { $select2 }
       *[s] Bar
}

## Variables:
## $select4 (Integer): a number

# Variables:
# $select3 (Integer): a number
select-with-comment1 = {
    $select3 ->
        [t] Foo
       *[s] Bar
}

select-with-group-comment1 = {
    $select4 ->
        [t] Foo { $select4 }
       *[s] Bar
}

message-attribute-without-comment =
    .label = This string as { $attr }

# Variables:
# $attr2 (String): just text
message-attribute-with-comment =
    .label = This string as { $attr2 }

message-selection-function =
  { PLATFORM() ->
      [macos] foo
     *[other] bar
  }
