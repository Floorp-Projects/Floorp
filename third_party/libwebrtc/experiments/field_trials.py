#!/usr/bin/env vpython3

# Copyright (c) 2022 The WebRTC Project Authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import datetime
from datetime import date
import sys
from typing import FrozenSet, Set

import argparse
import dataclasses


@dataclasses.dataclass(frozen=True)
class FieldTrial:
    """Representation of all attributes associated with a field trial.

    Attributes:
      key: Field trial key.
      bug: Associated open bug containing more context.
      end_date: Date when the field trial expires and must be deleted.
    """
    key: str
    bug: str
    end_date: date


# As per the policy in `g3doc/field-trials.md`, all field trials should be
# registered in the container below. Please keep the keys sorted.
REGISTERED_FIELD_TRIALS: FrozenSet[FieldTrial] = frozenset([
    FieldTrial('', '', date(1, 1, 1)),  # TODO(bugs.webrtc.org/14154): Populate
])


def registry_header(
        field_trials: FrozenSet[FieldTrial] = REGISTERED_FIELD_TRIALS) -> str:
    """Generates a C++ header with all field trial keys.

    Args:
      field_trials: Field trials to include in the header.

    Returns:
      String representation of a C++ header file containing all field trial
      keys.

    >>> trials = {
    ...     FieldTrial('B', '', date(1, 1, 1)),
    ...     FieldTrial('A', '', date(1, 1, 1)),
    ...     FieldTrial('B', '', date(2, 2, 2)),
    ... }
    >>> print(registry_header(trials))
    // This file was automatically generated. Do not edit.
    <BLANKLINE>
    #ifndef GEN_REGISTERED_FIELD_TRIALS_H_
    #define GEN_REGISTERED_FIELD_TRIALS_H_
    <BLANKLINE>
    #include "absl/strings/string_view.h"
    <BLANKLINE>
    namespace webrtc {
    <BLANKLINE>
    inline constexpr absl::string_view kRegisteredFieldTrials[] = {
        "A",
        "B",
    };
    <BLANKLINE>
    }  // namespace webrtc
    <BLANKLINE>
    #endif  // GEN_REGISTERED_FIELD_TRIALS_H_
    <BLANKLINE>
    """
    registered_keys = {f.key for f in field_trials}
    keys = '\n'.join(f'    "{k}",' for k in sorted(registered_keys))
    return ('// This file was automatically generated. Do not edit.\n'
            '\n'
            '#ifndef GEN_REGISTERED_FIELD_TRIALS_H_\n'
            '#define GEN_REGISTERED_FIELD_TRIALS_H_\n'
            '\n'
            '#include "absl/strings/string_view.h"\n'
            '\n'
            'namespace webrtc {\n'
            '\n'
            'inline constexpr absl::string_view kRegisteredFieldTrials[] = {\n'
            f'{keys}\n'
            '};\n'
            '\n'
            '}  // namespace webrtc\n'
            '\n'
            '#endif  // GEN_REGISTERED_FIELD_TRIALS_H_\n')


def expired_field_trials(
    threshold: date,
    field_trials: FrozenSet[FieldTrial] = REGISTERED_FIELD_TRIALS
) -> Set[FieldTrial]:
    """Obtains expired field trials.

    Args:
      threshold: Date from which to check end date.
      field_trials: Field trials to validate.

    Returns:
      All expired field trials.

    >>> trials = {
    ...     FieldTrial('Expired', '', date(1, 1, 1)),
    ...     FieldTrial('Not-Expired', '', date(1, 1, 2)),
    ... }
    >>> expired_field_trials(date(1, 1, 1), trials)
    {FieldTrial(key='Expired', bug='', end_date=datetime.date(1, 1, 1))}
    """
    return {f for f in field_trials if f.end_date <= threshold}


def cmd_header(args: argparse.Namespace) -> None:
    args.output.write(registry_header())


def cmd_expired(args: argparse.Namespace) -> None:
    now = datetime.datetime.now(datetime.timezone.utc)
    today = date(now.year, now.month, now.day)
    diff = datetime.timedelta(days=args.in_days)
    expired = expired_field_trials(today + diff)

    if len(expired) <= 0:
        return

    expired_by_date = sorted([(f.end_date, f.key) for f in expired])
    print('\n'.join(
        f'{key} {"expired" if date <= today else "expires"} on {date}'
        for date, key in expired_by_date))
    if any(date <= today for date, _ in expired_by_date):
        sys.exit(1)


def main() -> None:
    parser = argparse.ArgumentParser()
    subcommand = parser.add_subparsers(dest='cmd')

    parser_header = subcommand.add_parser(
        'header',
        help='generate C++ header file containing registered field trial keys')
    parser_header.add_argument('--output',
                               default=sys.stdout,
                               type=argparse.FileType('w'),
                               required=False,
                               help='output file')
    parser_header.set_defaults(cmd=cmd_header)

    parser_expired = subcommand.add_parser(
        'expired',
        help='lists all expired field trials',
        description='''
        Lists all expired field trials. Exits with a non-zero exit status if
        any field trials has expired, ignoring the --in-days argument.
        ''')
    parser_expired.add_argument(
        '--in-days',
        default=0,
        type=int,
        required=False,
        help='number of days relative to today to check')
    parser_expired.set_defaults(cmd=cmd_expired)

    args = parser.parse_args()

    if not args.cmd:
        parser.print_help(sys.stderr)
        sys.exit(1)

    args.cmd(args)


if __name__ == '__main__':
    main()
