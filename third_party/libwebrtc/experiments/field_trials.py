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
from typing import FrozenSet, List, Set

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
# registered in the container below.
ACTIVE_FIELD_TRIALS: FrozenSet[FieldTrial] = frozenset([
    # keep-sorted start
    FieldTrial('', '', date(1, 1, 1)),  # TODO(bugs.webrtc.org/14154): Populate
    # keep-sorted end
])

# These field trials precedes the policy in `g3doc/field-trials.md` and are
# therefore not required to follow it. Do not add any new field trials here.
POLICY_EXEMPT_FIELD_TRIALS: FrozenSet[FieldTrial] = frozenset([
    # keep-sorted start
    FieldTrial('', '', date(1, 1, 1)),  # TODO(bugs.webrtc.org/14154): Populate
    # keep-sorted end
])

REGISTERED_FIELD_TRIALS: FrozenSet[FieldTrial] = ACTIVE_FIELD_TRIALS.union(
    POLICY_EXEMPT_FIELD_TRIALS)


def todays_date() -> date:
    now = datetime.datetime.now(datetime.timezone.utc)
    return date(now.year, now.month, now.day)


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


def validate_field_trials(
        field_trials: FrozenSet[FieldTrial] = ACTIVE_FIELD_TRIALS
) -> List[str]:
    """Validate that field trials conforms to the policy.

    Args:
      field_trials: Field trials to validate.

    Returns:
      A list of explanations for invalid field trials.
    """
    invalid = []
    for trial in field_trials:
        if not trial.key.startswith('WebRTC-'):
            invalid.append(f'{trial.key} does not start with "WebRTC-".')
        if len(trial.bug) <= 0:
            invalid.append(f'{trial.key} must have an associated bug.')
    return invalid


def cmd_header(args: argparse.Namespace) -> None:
    args.output.write(registry_header())


def cmd_expired(args: argparse.Namespace) -> None:
    today = todays_date()
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


def cmd_validate(args: argparse.Namespace) -> None:
    del args
    invalid = validate_field_trials()

    if len(invalid) <= 0:
        return

    print('\n'.join(sorted(invalid)))
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

    parser_validate = subcommand.add_parser(
        'validate',
        help='validates that all field trials conforms to the policy.',
        description='''
        Validates that all field trials conforms to the policy. Exits with a
        non-zero exit status if any field trials does not.
        ''')
    parser_validate.set_defaults(cmd=cmd_validate)

    args = parser.parse_args()

    if not args.cmd:
        parser.print_help(sys.stderr)
        sys.exit(1)

    args.cmd(args)


if __name__ == '__main__':
    main()
