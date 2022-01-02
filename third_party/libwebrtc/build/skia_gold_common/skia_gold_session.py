# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Class for interacting with the Skia Gold image diffing service."""

import logging
import os
import shutil
import subprocess
import sys
import tempfile

CHROMIUM_SRC = os.path.realpath(
    os.path.join(os.path.dirname(__file__), '..', '..'))

GOLDCTL_BINARY = os.path.join(CHROMIUM_SRC, 'tools', 'skia_goldctl')
if sys.platform == 'win32':
  GOLDCTL_BINARY = os.path.join(GOLDCTL_BINARY, 'win', 'goldctl') + '.exe'
elif sys.platform == 'darwin':
  GOLDCTL_BINARY = os.path.join(GOLDCTL_BINARY, 'mac', 'goldctl')
else:
  GOLDCTL_BINARY = os.path.join(GOLDCTL_BINARY, 'linux', 'goldctl')


class SkiaGoldSession(object):
  class StatusCodes(object):
    """Status codes for RunComparison."""
    SUCCESS = 0
    AUTH_FAILURE = 1
    INIT_FAILURE = 2
    COMPARISON_FAILURE_REMOTE = 3
    COMPARISON_FAILURE_LOCAL = 4
    LOCAL_DIFF_FAILURE = 5
    NO_OUTPUT_MANAGER = 6

  class ComparisonResults(object):
    """Struct-like object for storing results of an image comparison."""

    def __init__(self):
      self.public_triage_link = None
      self.internal_triage_link = None
      self.triage_link_omission_reason = None
      self.local_diff_given_image = None
      self.local_diff_closest_image = None
      self.local_diff_diff_image = None

  def __init__(self, working_dir, gold_properties, keys_file, corpus, instance):
    """Abstract class to handle all aspects of image comparison via Skia Gold.

    A single SkiaGoldSession is valid for a single instance/corpus/keys_file
    combination.

    Args:
      working_dir: The directory to store config files, etc.
      gold_properties: A skia_gold_properties.SkiaGoldProperties instance for
          the current test run.
      keys_file: A path to a JSON file containing various comparison config data
          such as corpus and debug information like the hardware/software
          configuration the images will be produced on.
      corpus: The corpus that images that will be compared belong to.
      instance: The name of the Skia Gold instance to interact with.
    """
    self._working_dir = working_dir
    self._gold_properties = gold_properties
    self._keys_file = keys_file
    self._corpus = corpus
    self._instance = instance
    self._triage_link_file = tempfile.NamedTemporaryFile(suffix='.txt',
                                                         dir=working_dir,
                                                         delete=False).name
    # A map of image name (string) to ComparisonResults for that image.
    self._comparison_results = {}
    self._authenticated = False
    self._initialized = False

  def RunComparison(self,
                    name,
                    png_file,
                    output_manager,
                    inexact_matching_args=None,
                    use_luci=True):
    """Helper method to run all steps to compare a produced image.

    Handles authentication, itnitialization, comparison, and, if necessary,
    local diffing.

    Args:
      name: The name of the image being compared.
      png_file: A path to a PNG file containing the image to be compared.
      output_manager: An output manager to use to store diff links. The
          argument's type depends on what type a subclasses' _StoreDiffLinks
          implementation expects. Can be None even if _StoreDiffLinks expects
          a valid input, but will fail if it ever actually needs to be used.
      inexact_matching_args: A list of strings containing extra command line
          arguments to pass to Gold for inexact matching. Can be omitted to use
          exact matching.
      use_luci: If true, authentication will use the service account provided by
          the LUCI context. If false, will attempt to use whatever is set up in
          gsutil, which is only supported for local runs.

    Returns:
      A tuple (status, error). |status| is a value from
      SkiaGoldSession.StatusCodes signifying the result of the comparison.
      |error| is an error message describing the status if not successful.
    """
    auth_rc, auth_stdout = self.Authenticate(use_luci=use_luci)
    if auth_rc:
      return self.StatusCodes.AUTH_FAILURE, auth_stdout

    init_rc, init_stdout = self.Initialize()
    if init_rc:
      return self.StatusCodes.INIT_FAILURE, init_stdout

    compare_rc, compare_stdout = self.Compare(
        name=name,
        png_file=png_file,
        inexact_matching_args=inexact_matching_args)
    if not compare_rc:
      return self.StatusCodes.SUCCESS, None

    logging.error('Gold comparison failed: %s', compare_stdout)
    if not self._gold_properties.local_pixel_tests:
      return self.StatusCodes.COMPARISON_FAILURE_REMOTE, compare_stdout

    if not output_manager:
      return (self.StatusCodes.NO_OUTPUT_MANAGER,
              'No output manager for local diff images')

    diff_rc, diff_stdout = self.Diff(name=name,
                                     png_file=png_file,
                                     output_manager=output_manager)
    if diff_rc:
      return self.StatusCodes.LOCAL_DIFF_FAILURE, diff_stdout
    return self.StatusCodes.COMPARISON_FAILURE_LOCAL, compare_stdout

  def Authenticate(self, use_luci=True):
    """Authenticates with Skia Gold for this session.

    Args:
      use_luci: If true, authentication will use the service account provided
          by the LUCI context. If false, will attempt to use whatever is set up
          in gsutil, which is only supported for local runs.

    Returns:
      A tuple (return_code, output). |return_code| is the return code of the
      authentication process. |output| is the stdout + stderr of the
      authentication process.
    """
    if self._authenticated:
      return 0, None
    if self._gold_properties.bypass_skia_gold_functionality:
      logging.warning('Not actually authenticating with Gold due to '
                      '--bypass-skia-gold-functionality being present.')
      return 0, None

    auth_cmd = [GOLDCTL_BINARY, 'auth', '--work-dir', self._working_dir]
    if use_luci:
      auth_cmd.append('--luci')
    elif not self._gold_properties.local_pixel_tests:
      raise RuntimeError(
          'Cannot authenticate to Skia Gold with use_luci=False unless running '
          'local pixel tests')

    rc, stdout = self._RunCmdForRcAndOutput(auth_cmd)
    if rc == 0:
      self._authenticated = True
    return rc, stdout

  def Initialize(self):
    """Initializes the working directory if necessary.

    This can technically be skipped if the same information is passed to the
    command used for image comparison, but that is less efficient under the
    hood. Doing it that way effectively requires an initialization for every
    comparison (~250 ms) instead of once at the beginning.

    Returns:
      A tuple (return_code, output). |return_code| is the return code of the
      initialization process. |output| is the stdout + stderr of the
      initialization process.
    """
    if self._initialized:
      return 0, None
    if self._gold_properties.bypass_skia_gold_functionality:
      logging.warning('Not actually initializing Gold due to '
                      '--bypass-skia-gold-functionality being present.')
      return 0, None

    init_cmd = [
        GOLDCTL_BINARY,
        'imgtest',
        'init',
        '--passfail',
        '--instance',
        self._instance,
        '--corpus',
        self._corpus,
        '--keys-file',
        self._keys_file,
        '--work-dir',
        self._working_dir,
        '--failure-file',
        self._triage_link_file,
        '--commit',
        self._gold_properties.git_revision,
    ]
    if self._gold_properties.IsTryjobRun():
      init_cmd.extend([
          '--issue',
          str(self._gold_properties.issue),
          '--patchset',
          str(self._gold_properties.patchset),
          '--jobid',
          str(self._gold_properties.job_id),
          '--crs',
          str(self._gold_properties.code_review_system),
          '--cis',
          str(self._gold_properties.continuous_integration_system),
      ])

    rc, stdout = self._RunCmdForRcAndOutput(init_cmd)
    if rc == 0:
      self._initialized = True
    return rc, stdout

  def Compare(self, name, png_file, inexact_matching_args=None):
    """Compares the given image to images known to Gold.

    Triage links can later be retrieved using GetTriageLinks().

    Args:
      name: The name of the image being compared.
      png_file: A path to a PNG file containing the image to be compared.
      inexact_matching_args: A list of strings containing extra command line
          arguments to pass to Gold for inexact matching. Can be omitted to use
          exact matching.

    Returns:
      A tuple (return_code, output). |return_code| is the return code of the
      comparison process. |output| is the stdout + stderr of the comparison
      process.
    """
    if self._gold_properties.bypass_skia_gold_functionality:
      logging.warning('Not actually comparing with Gold due to '
                      '--bypass-skia-gold-functionality being present.')
      return 0, None

    compare_cmd = [
        GOLDCTL_BINARY,
        'imgtest',
        'add',
        '--test-name',
        name,
        '--png-file',
        png_file,
        '--work-dir',
        self._working_dir,
    ]
    if self._gold_properties.local_pixel_tests:
      compare_cmd.append('--dryrun')
    if inexact_matching_args:
      logging.info('Using inexact matching arguments for image %s: %s', name,
                   inexact_matching_args)
      compare_cmd.extend(inexact_matching_args)

    self._ClearTriageLinkFile()
    rc, stdout = self._RunCmdForRcAndOutput(compare_cmd)

    self._comparison_results[name] = self.ComparisonResults()
    if rc == 0:
      self._comparison_results[name].triage_link_omission_reason = (
          'Comparison succeeded, no triage link')
    elif self._gold_properties.IsTryjobRun():
      cl_triage_link = ('https://{instance}-gold.skia.org/cl/{crs}/{issue}')
      cl_triage_link = cl_triage_link.format(
          instance=self._instance,
          crs=self._gold_properties.code_review_system,
          issue=self._gold_properties.issue)
      self._comparison_results[name].internal_triage_link = cl_triage_link
      self._comparison_results[name].public_triage_link =\
          self._GeneratePublicTriageLink(cl_triage_link)
    else:
      try:
        with open(self._triage_link_file) as tlf:
          triage_link = tlf.read().strip()
        if not triage_link:
          self._comparison_results[name].triage_link_omission_reason = (
              'Gold did not provide a triage link. This is likely a bug on '
              "Gold's end.")
          self._comparison_results[name].internal_triage_link = None
          self._comparison_results[name].public_triage_link = None
        else:
          self._comparison_results[name].internal_triage_link = triage_link
          self._comparison_results[name].public_triage_link =\
              self._GeneratePublicTriageLink(triage_link)
      except IOError:
        self._comparison_results[name].triage_link_omission_reason = (
            'Failed to read triage link from file')
    return rc, stdout

  def Diff(self, name, png_file, output_manager):
    """Performs a local image diff against the closest known positive in Gold.

    This is used for running tests on a workstation, where uploading data to
    Gold for ingestion is not allowed, and thus the web UI is not available.

    Image links can later be retrieved using Get*ImageLink().

    Args:
      name: The name of the image being compared.
      png_file: The path to a PNG file containing the image to be diffed.
      output_manager: An output manager to use to store diff links. The
          argument's type depends on what type a subclasses' _StoreDiffLinks
          implementation expects.

    Returns:
      A tuple (return_code, output). |return_code| is the return code of the
      diff process. |output| is the stdout + stderr of the diff process.
    """
    # Instead of returning that everything is okay and putting in dummy links,
    # just fail since this should only be called when running locally and
    # --bypass-skia-gold-functionality is only meant for use on the bots.
    if self._gold_properties.bypass_skia_gold_functionality:
      raise RuntimeError(
          '--bypass-skia-gold-functionality is not supported when running '
          'tests locally.')

    output_dir = self._CreateDiffOutputDir()
    # TODO(skbug.com/10611): Remove this temporary work dir and instead just use
    # self._working_dir once `goldctl diff` stops clobbering the auth files in
    # the provided work directory.
    temp_work_dir = tempfile.mkdtemp()
    # shutil.copytree() fails if the destination already exists, so use a
    # subdirectory of the temporary directory.
    temp_work_dir = os.path.join(temp_work_dir, 'diff_work_dir')
    try:
      shutil.copytree(self._working_dir, temp_work_dir)
      diff_cmd = [
          GOLDCTL_BINARY,
          'diff',
          '--corpus',
          self._corpus,
          '--instance',
          # TODO(skbug.com/10610): Decide whether to use the public or
          # non-public instance once authentication is fixed for the non-public
          # instance.
          str(self._instance) + '-public',
          '--input',
          png_file,
          '--test',
          name,
          '--work-dir',
          temp_work_dir,
          '--out-dir',
          output_dir,
      ]
      rc, stdout = self._RunCmdForRcAndOutput(diff_cmd)
      self._StoreDiffLinks(name, output_manager, output_dir)
      return rc, stdout
    finally:
      shutil.rmtree(os.path.realpath(os.path.join(temp_work_dir, '..')))

  def GetTriageLinks(self, name):
    """Gets the triage links for the given image.

    Args:
      name: The name of the image to retrieve the triage link for.

    Returns:
      A tuple (public, internal). |public| is a string containing the triage
      link for the public Gold instance if it is available, or None if it is not
      available for some reason. |internal| is the same as |public|, but
      containing a link to the internal Gold instance. The reason for links not
      being available can be retrieved using GetTriageLinkOmissionReason.
    """
    comparison_results = self._comparison_results.get(name,
                                                      self.ComparisonResults())
    return (comparison_results.public_triage_link,
            comparison_results.internal_triage_link)

  def GetTriageLinkOmissionReason(self, name):
    """Gets the reason why a triage link is not available for an image.

    Args:
      name: The name of the image whose triage link does not exist.

    Returns:
      A string containing the reason why a triage link is not available.
    """
    if name not in self._comparison_results:
      return 'No image comparison performed for %s' % name
    results = self._comparison_results[name]
    # This method should not be called if there is a valid triage link.
    assert results.public_triage_link is None
    assert results.internal_triage_link is None
    if results.triage_link_omission_reason:
      return results.triage_link_omission_reason
    if results.local_diff_given_image:
      return 'Gold only used to do a local image diff'
    raise RuntimeError(
        'Somehow have a ComparisonResults instance for %s that should not '
        'exist' % name)

  def GetGivenImageLink(self, name):
    """Gets the link to the given image used for local diffing.

    Args:
      name: The name of the image that was diffed.

    Returns:
      A string containing the link to where the image is saved, or None if it
      does not exist.
    """
    assert name in self._comparison_results
    return self._comparison_results[name].local_diff_given_image

  def GetClosestImageLink(self, name):
    """Gets the link to the closest known image used for local diffing.

    Args:
      name: The name of the image that was diffed.

    Returns:
      A string containing the link to where the image is saved, or None if it
      does not exist.
    """
    assert name in self._comparison_results
    return self._comparison_results[name].local_diff_closest_image

  def GetDiffImageLink(self, name):
    """Gets the link to the diff between the given and closest images.

    Args:
      name: The name of the image that was diffed.

    Returns:
      A string containing the link to where the image is saved, or None if it
      does not exist.
    """
    assert name in self._comparison_results
    return self._comparison_results[name].local_diff_diff_image

  def _GeneratePublicTriageLink(self, internal_link):
    """Generates a public triage link given an internal one.

    Args:
      internal_link: A string containing a triage link pointing to an internal
          Gold instance.

    Returns:
      A string containing a triage link pointing to the public mirror of the
      link pointed to by |internal_link|.
    """
    return internal_link.replace('%s-gold' % self._instance,
                                 '%s-public-gold' % self._instance)

  def _ClearTriageLinkFile(self):
    """Clears the contents of the triage link file.

    This should be done before every comparison since goldctl appends to the
    file instead of overwriting its contents, which results in multiple triage
    links getting concatenated together if there are multiple failures.
    """
    open(self._triage_link_file, 'w').close()

  def _CreateDiffOutputDir(self):
    return tempfile.mkdtemp(dir=self._working_dir)

  def _StoreDiffLinks(self, image_name, output_manager, output_dir):
    """Stores the local diff files as links.

    The ComparisonResults entry for |image_name| should have its *_image fields
    filled after this unless corresponding images were not found on disk.

    Args:
      image_name: A string containing the name of the image that was diffed.
      output_manager: An output manager used used to surface links to users,
          if necessary. The expected argument type depends on each subclasses'
          implementation of this method.
      output_dir: A string containing the path to the directory where diff
          output image files where saved.
    """
    raise NotImplementedError()

  @staticmethod
  def _RunCmdForRcAndOutput(cmd):
    """Runs |cmd| and returns its returncode and output.

    Args:
      cmd: A list containing the command line to run.

    Returns:
      A tuple (rc, output), where, |rc| is the returncode of the command and
      |output| is the stdout + stderr of the command.
    """
    raise NotImplementedError()
