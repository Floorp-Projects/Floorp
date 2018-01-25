# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import datetime
import fnmatch
import glob
import os
import plistlib
import shutil
import subprocess
import sys
import tempfile


def GetProvisioningProfilesDir():
  """Returns the location of the installed mobile provisioning profiles.

  Returns:
    The path to the directory containing the installed mobile provisioning
    profiles as a string.
  """
  return os.path.join(
      os.environ['HOME'], 'Library', 'MobileDevice', 'Provisioning Profiles')


def LoadPlistFile(plist_path):
  """Loads property list file at |plist_path|.

  Args:
    plist_path: path to the property list file to load.

  Returns:
    The content of the property list file as a python object.
  """
  return plistlib.readPlistFromString(subprocess.check_output([
      'xcrun', 'plutil', '-convert', 'xml1', '-o', '-', plist_path]))


class Bundle(object):
  """Wraps a bundle."""

  def __init__(self, bundle_path):
    """Initializes the Bundle object with data from bundle Info.plist file."""
    self._path = bundle_path
    self._data = LoadPlistFile(os.path.join(self._path, 'Info.plist'))

  @property
  def path(self):
    return self._path

  @property
  def identifier(self):
    return self._data['CFBundleIdentifier']

  @property
  def binary_path(self):
    return os.path.join(self._path, self._data['CFBundleExecutable'])


class ProvisioningProfile(object):
  """Wraps a mobile provisioning profile file."""

  def __init__(self, provisioning_profile_path):
    """Initializes the ProvisioningProfile with data from profile file."""
    self._path = provisioning_profile_path
    self._data = plistlib.readPlistFromString(subprocess.check_output([
        'xcrun', 'security', 'cms', '-D', '-u', 'certUsageAnyCA',
        '-i', provisioning_profile_path]))

  @property
  def path(self):
    return self._path

  @property
  def application_identifier_pattern(self):
    return self._data.get('Entitlements', {}).get('application-identifier', '')

  @property
  def team_identifier(self):
    return self._data.get('TeamIdentifier', [''])[0]

  @property
  def entitlements(self):
    return self._data.get('Entitlements', {})

  @property
  def expiration_date(self):
    return self._data.get('ExpirationDate', datetime.datetime.now())

  def ValidToSignBundle(self, bundle_identifier):
    """Checks whether the provisioning profile can sign bundle_identifier.

    Args:
      bundle_identifier: the identifier of the bundle that needs to be signed.

    Returns:
      True if the mobile provisioning profile can be used to sign a bundle
      with the corresponding bundle_identifier, False otherwise.
    """
    return fnmatch.fnmatch(
        '%s.%s' % (self.team_identifier, bundle_identifier),
        self.application_identifier_pattern)

  def Install(self, installation_path):
    """Copies mobile provisioning profile info to |installation_path|."""
    shutil.copy2(self.path, installation_path)


class Entitlements(object):
  """Wraps an Entitlement plist file."""

  def __init__(self, entitlements_path):
    """Initializes Entitlements object from entitlement file."""
    self._path = entitlements_path
    self._data = LoadPlistFile(self._path)

  @property
  def path(self):
    return self._path

  def ExpandVariables(self, substitutions):
    self._data = self._ExpandVariables(self._data, substitutions)

  def _ExpandVariables(self, data, substitutions):
    if isinstance(data, str):
      for key, substitution in substitutions.iteritems():
        data = data.replace('$(%s)' % (key,), substitution)
      return data

    if isinstance(data, dict):
      for key, value in data.iteritems():
        data[key] = self._ExpandVariables(value, substitutions)
      return data

    if isinstance(data, list):
      for i, value in enumerate(data):
        data[i] = self._ExpandVariables(value, substitutions)

    return data

  def LoadDefaults(self, defaults):
    for key, value in defaults.iteritems():
      if key not in self._data:
        self._data[key] = value

  def WriteTo(self, target_path):
    plistlib.writePlist(self._data, target_path)


def FindProvisioningProfile(bundle_identifier, required):
  """Finds mobile provisioning profile to use to sign bundle.

  Args:
    bundle_identifier: the identifier of the bundle to sign.

  Returns:
    The ProvisioningProfile object that can be used to sign the Bundle
    object or None if no matching provisioning profile was found.
  """
  provisioning_profile_paths = glob.glob(
      os.path.join(GetProvisioningProfilesDir(), '*.mobileprovision'))

  # Iterate over all installed mobile provisioning profiles and filter those
  # that can be used to sign the bundle, ignoring expired ones.
  now = datetime.datetime.now()
  valid_provisioning_profiles = []
  one_hour = datetime.timedelta(0, 3600)
  for provisioning_profile_path in provisioning_profile_paths:
    provisioning_profile = ProvisioningProfile(provisioning_profile_path)
    if provisioning_profile.expiration_date - now < one_hour:
      sys.stderr.write(
          'Warning: ignoring expired provisioning profile: %s.\n' %
          provisioning_profile_path)
      continue
    if provisioning_profile.ValidToSignBundle(bundle_identifier):
      valid_provisioning_profiles.append(provisioning_profile)

  if not valid_provisioning_profiles:
    if required:
      sys.stderr.write(
          'Error: no mobile provisioning profile found for "%s".\n' %
          bundle_identifier)
      sys.exit(1)
    return None

  # Select the most specific mobile provisioning profile, i.e. the one with
  # the longest application identifier pattern (prefer the one with the latest
  # expiration date as a secondary criteria).
  selected_provisioning_profile = max(
      valid_provisioning_profiles,
      key=lambda p: (len(p.application_identifier_pattern), p.expiration_date))

  one_week = datetime.timedelta(7)
  if selected_provisioning_profile.expiration_date - now < 2 * one_week:
    sys.stderr.write(
        'Warning: selected provisioning profile will expire soon: %s' %
        selected_provisioning_profile.path)
  return selected_provisioning_profile


def CodeSignBundle(bundle_path, identity, extra_args):
  process = subprocess.Popen(['xcrun', 'codesign', '--force', '--sign',
      identity, '--timestamp=none'] + list(extra_args) + [bundle_path],
      stderr=subprocess.PIPE)
  _, stderr = process.communicate()
  if process.returncode:
    sys.stderr.write(stderr)
    sys.exit(process.returncode)
  for line in stderr.splitlines():
    if line.endswith(': replacing existing signature'):
      # Ignore warning about replacing existing signature as this should only
      # happen when re-signing system frameworks (and then it is expected).
      continue
    sys.stderr.write(line)
    sys.stderr.write('\n')


def InstallSystemFramework(framework_path, bundle_path, args):
  """Install framework from |framework_path| to |bundle| and code-re-sign it."""
  installed_framework_path = os.path.join(
      bundle_path, 'Frameworks', os.path.basename(framework_path))

  if os.path.exists(installed_framework_path):
    shutil.rmtree(installed_framework_path)

  shutil.copytree(framework_path, installed_framework_path)
  CodeSignBundle(installed_framework_path, args.identity,
      ['--deep', '--preserve-metadata=identifier,entitlements'])


def GenerateEntitlements(path, provisioning_profile, bundle_identifier):
  """Generates an entitlements file.

  Args:
    path: path to the entitlements template file
    provisioning_profile: ProvisioningProfile object to use, may be None
    bundle_identifier: identifier of the bundle to sign.
  """
  entitlements = Entitlements(path)
  if provisioning_profile:
    entitlements.LoadDefaults(provisioning_profile.entitlements)
    app_identifier_prefix = provisioning_profile.team_identifier + '.'
  else:
    app_identifier_prefix = '*.'
  entitlements.ExpandVariables({
      'CFBundleIdentifier': bundle_identifier,
      'AppIdentifierPrefix': app_identifier_prefix,
  })
  return entitlements


class Action(object):
  """Class implementing one action supported by the script."""

  @classmethod
  def Register(cls, subparsers):
    parser = subparsers.add_parser(cls.name, help=cls.help)
    parser.set_defaults(func=cls._Execute)
    cls._Register(parser)


class CodeSignBundleAction(Action):
  """Class implementing the code-sign-bundle action."""

  name = 'code-sign-bundle'
  help = 'perform code signature for a bundle'

  @staticmethod
  def _Register(parser):
    parser.add_argument(
        '--entitlements', '-e', dest='entitlements_path',
        help='path to the entitlements file to use')
    parser.add_argument(
        'path', help='path to the iOS bundle to codesign')
    parser.add_argument(
        '--identity', '-i', required=True,
        help='identity to use to codesign')
    parser.add_argument(
        '--binary', '-b', required=True,
        help='path to the iOS bundle binary')
    parser.add_argument(
        '--framework', '-F', action='append', default=[], dest='frameworks',
        help='install and resign system framework')
    parser.add_argument(
        '--disable-code-signature', action='store_true', dest='no_signature',
        help='disable code signature')
    parser.add_argument(
        '--platform', '-t', required=True,
        help='platform the signed bundle is targetting')
    parser.set_defaults(no_signature=False)

  @staticmethod
  def _Execute(args):
    if not args.identity:
      args.identity = '-'

    bundle = Bundle(args.path)

    # Delete existing embedded mobile provisioning.
    embedded_provisioning_profile = os.path.join(
        bundle.path, 'embedded.mobileprovision')
    if os.path.isfile(embedded_provisioning_profile):
      os.unlink(embedded_provisioning_profile)

    # Delete existing code signature.
    signature_file = os.path.join(args.path, '_CodeSignature', 'CodeResources')
    if os.path.isfile(signature_file):
      shutil.rmtree(os.path.dirname(signature_file))

    # Install system frameworks if requested.
    for framework_path in args.frameworks:
      InstallSystemFramework(framework_path, args.path, args)

    # Copy main binary into bundle.
    if os.path.isfile(bundle.binary_path):
      os.unlink(bundle.binary_path)
    shutil.copy(args.binary, bundle.binary_path)

    if args.no_signature:
      return

    codesign_extra_args = []

    # Find mobile provisioning profile and embeds it into the bundle (if a code
    # signing identify has been provided, fails if no valid mobile provisioning
    # is found).
    provisioning_profile_required = args.identity != '-'
    provisioning_profile = FindProvisioningProfile(
        bundle.identifier, provisioning_profile_required)
    if provisioning_profile and args.platform != 'iphonesimulator':
      provisioning_profile.Install(embedded_provisioning_profile)

      temporary_entitlements_file = tempfile.NamedTemporaryFile(suffix='.xcent')
      codesign_extra_args.extend(
          ['--entitlements', temporary_entitlements_file.name])

      entitlements = GenerateEntitlements(
          args.entitlements_path, provisioning_profile, bundle.identifier)
      entitlements.WriteTo(temporary_entitlements_file.name)

    CodeSignBundle(bundle.path, args.identity, codesign_extra_args)


class CodeSignFileAction(Action):
  """Class implementing code signature for a single file."""

  name = 'code-sign-file'
  help = 'code-sign a single file'

  @staticmethod
  def _Register(parser):
    parser.add_argument(
        'path', help='path to the file to codesign')
    parser.add_argument(
        '--identity', '-i', required=True,
        help='identity to use to codesign')
    parser.add_argument(
        '--output', '-o',
        help='if specified copy the file to that location before signing it')
    parser.set_defaults(sign=True)

  @staticmethod
  def _Execute(args):
    if not args.identity:
      args.identity = '-'

    install_path = args.path
    if args.output:

      if os.path.isfile(args.output):
        os.unlink(args.output)
      elif os.path.isdir(args.output):
        shutil.rmtree(args.output)

      if os.path.isfile(args.path):
        shutil.copy(args.path, args.output)
      elif os.path.isdir(args.path):
        shutil.copytree(args.path, args.output)

      install_path = args.output

    CodeSignBundle(install_path, args.identity,
      ['--deep', '--preserve-metadata=identifier,entitlements'])


class GenerateEntitlementsAction(Action):
  """Class implementing the generate-entitlements action."""

  name = 'generate-entitlements'
  help = 'generate entitlements file'

  @staticmethod
  def _Register(parser):
    parser.add_argument(
        '--entitlements', '-e', dest='entitlements_path',
        help='path to the entitlements file to use')
    parser.add_argument(
        'path', help='path to the entitlements file to generate')
    parser.add_argument(
        '--info-plist', '-p', required=True,
        help='path to the bundle Info.plist')

  @staticmethod
  def _Execute(args):
    info_plist = LoadPlistFile(args.info_plist)
    bundle_identifier = info_plist['CFBundleIdentifier']
    provisioning_profile = FindProvisioningProfile(bundle_identifier, False)
    entitlements = GenerateEntitlements(
        args.entitlements_path, provisioning_profile, bundle_identifier)
    entitlements.WriteTo(args.path)


def Main():
  parser = argparse.ArgumentParser('codesign iOS bundles')
  parser.add_argument('--developer_dir', required=False,
                      help='Path to Xcode.')
  subparsers = parser.add_subparsers()

  actions = [
      CodeSignBundleAction,
      CodeSignFileAction,
      GenerateEntitlementsAction,
  ]

  for action in actions:
    action.Register(subparsers)

  args = parser.parse_args()
  if args.developer_dir:
    os.environ['DEVELOPER_DIR'] = args.developer_dir
  args.func(args)


if __name__ == '__main__':
  sys.exit(Main())
