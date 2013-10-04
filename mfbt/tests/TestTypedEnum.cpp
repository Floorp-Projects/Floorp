/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/TypedEnum.h"

MOZ_BEGIN_ENUM_CLASS(AutoEnum)
  A,
  B
MOZ_END_ENUM_CLASS(AutoEnum)


MOZ_BEGIN_ENUM_CLASS(CharEnum, char)
  A,
  B
MOZ_END_ENUM_CLASS(CharEnum)

struct Nested
{
  MOZ_BEGIN_NESTED_ENUM_CLASS(AutoEnum)
    C
  MOZ_END_NESTED_ENUM_CLASS(AutoEnum)

  MOZ_BEGIN_NESTED_ENUM_CLASS(CharEnum, char)
    D = 4,
    E
  MOZ_END_NESTED_ENUM_CLASS(CharEnum)
};

// Simply check that this compiles.
const AutoEnum autoEnum = AutoEnum::A;
const CharEnum charEnum = CharEnum::B;
const Nested::AutoEnum nestedAutoEnum = Nested::AutoEnum::C;
const Nested::CharEnum nestedCharEnum = Nested::CharEnum::D;

int
main()
{
}
