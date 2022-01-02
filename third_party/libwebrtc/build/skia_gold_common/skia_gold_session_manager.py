# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Class for managing multiple SkiaGoldSessions."""

import json
import tempfile


class SkiaGoldSessionManager(object):
  def __init__(self, working_dir, gold_properties):
    """Abstract class to manage one or more skia_gold_session.SkiaGoldSessions.

    A separate session is required for each instance/corpus/keys_file
    combination, so this class will lazily create them as necessary.

    Args:
      working_dir: The working directory under which each individual
          SkiaGoldSessions' working directory will be created.
      gold_properties: A SkiaGoldProperties instance that will be used to create
          any SkiaGoldSessions.
    """
    self._working_dir = working_dir
    self._gold_properties = gold_properties
    self._sessions = {}

  def GetSkiaGoldSession(self, keys_input, corpus=None, instance=None):
    """Gets a SkiaGoldSession for the given arguments.

    Lazily creates one if necessary.

    Args:
      keys_input: A way of retrieving various comparison config data such as
          corpus and debug information like the hardware/software configuration
          the image was produced on. Can be either a dict or a filepath to a
          file containing JSON to read.
      corpus: A string containing the corpus the session is for. If None, the
          corpus will be determined using available information.
      instance: The name of the Skia Gold instance to interact with. It None,
          will use whatever default the subclass sets.
    """
    instance = instance or self._GetDefaultInstance()
    keys_dict = _GetKeysAsDict(keys_input)
    keys_string = json.dumps(keys_dict, sort_keys=True)
    if corpus is None:
      corpus = keys_dict.get('source_type', instance)
    # Use the string representation of the keys JSON as a proxy for a hash since
    # dicts themselves are not hashable.
    session = self._sessions.setdefault(instance,
                                        {}).setdefault(corpus, {}).setdefault(
                                            keys_string, None)
    if not session:
      working_dir = tempfile.mkdtemp(dir=self._working_dir)
      keys_file = _GetKeysAsJson(keys_input, working_dir)
      session = self._GetSessionClass()(working_dir, self._gold_properties,
                                        keys_file, corpus, instance)
      self._sessions[instance][corpus][keys_string] = session
    return session

  @staticmethod
  def _GetDefaultInstance():
    """Gets the default Skia Gold instance.

    Returns:
      A string containing the default instance.
    """
    raise NotImplementedError

  @staticmethod
  def _GetSessionClass():
    """Gets the SkiaGoldSession class to use for session creation.

    Returns:
      A reference to a SkiaGoldSession class.
    """
    raise NotImplementedError


def _GetKeysAsDict(keys_input):
  """Converts |keys_input| into a dictionary.

  Args:
    keys_input: A dictionary or a string pointing to a JSON file. The contents
        of either should be Skia Gold config data.

  Returns:
    A dictionary containing the Skia Gold config data.
  """
  if isinstance(keys_input, dict):
    return keys_input
  assert isinstance(keys_input, str)
  with open(keys_input) as f:
    return json.load(f)


def _GetKeysAsJson(keys_input, session_work_dir):
  """Converts |keys_input| into a JSON file on disk.

  Args:
    keys_input: A dictionary or a string pointing to a JSON file. The contents
        of either should be Skia Gold config data.

  Returns:
    A string containing a filepath to a JSON file with containing |keys_input|'s
    data.
  """
  if isinstance(keys_input, str):
    return keys_input
  assert isinstance(keys_input, dict)
  keys_file = tempfile.NamedTemporaryFile(suffix='.json',
                                          dir=session_work_dir,
                                          delete=False).name
  with open(keys_file, 'w') as f:
    json.dump(keys_input, f)
  return keys_file
