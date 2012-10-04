#!/usr/bin/env python
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

import random
import string


class MissingParameterException(Exception):
  pass


def FillInParameter(parameter, value, template):
  if parameter not in template:
    raise MissingParameterException('Did not find parameter %s in template.' %
                                    parameter)

  return template.replace(parameter, value)


def RandomIdentifier():
  length = random.randint(1, 25)
  return (random.choice(string.letters) +
          ''.join(random.choice(string.letters + string.digits)
                  for i in xrange(length)))


def GenerateRandomJavascriptAttributes(num_attributes):
  return ['%s: %s' % (RandomIdentifier(), GenerateRandomJavascriptValue())
          for i in xrange(num_attributes)]


def MakeJavascriptObject(attributes):
  return '{ ' + ', '.join(attributes) + ' }'


def GenerateRandomJavascriptFunction():
  num_parameters = random.randint(0, 10)
  parameter_list = ', '.join(RandomIdentifier() for i in xrange(num_parameters))
  return 'function ' + RandomIdentifier() + '(' + parameter_list + ')' + '{ }'


def GenerateRandomJavascriptValue():
  roll = random.random()
  if roll < 0.3:
    return '"' + RandomIdentifier() + '"'
  elif roll < 0.6:
    return str(random.randint(-10000000, 10000000))
  elif roll < 0.9:
    # Functions are first-class objects.
    return GenerateRandomJavascriptFunction()
  else:
    return 'true' if random.random() < 0.5 else 'false'


def Fuzz(template):
  """Generates a single random HTML page which tries to mess with getUserMedia.

  We require a template which has certain placeholders defined in it (such
  as FUZZ_USER_MEDIA_INPUT). We then replace these placeholders with random
  identifiers and data in certain patterns. For instance, since the getUserMedia
  function accepts an object, we try to pass in everything from {video:true,
  audio:false} (which is a correct value) to {sdjkjsjh34sd:455, video:'yxuhsd'}
  and other strange things.

  See the template at corpus/template.html for an example of how a template
  looks like.
  """
  random.seed()
  attributes = GenerateRandomJavascriptAttributes(random.randint(0, 10))
  if (random.random() < 0.8):
    attributes.append('video: %s' % GenerateRandomJavascriptValue())
  if (random.random() < 0.8):
    attributes.append('audio: %s' % GenerateRandomJavascriptValue())
  input_object = MakeJavascriptObject(attributes)
  template = FillInParameter('FUZZ_USER_MEDIA_INPUT', input_object, template)

  ok_callback = (GenerateRandomJavascriptValue()
                 if random.random() < 0.5 else 'getUserMediaOkCallback')
  template = FillInParameter('FUZZ_OK_CALLBACK', ok_callback, template)

  fail_callback = (GenerateRandomJavascriptValue()
                   if random.random() < 0.5 else 'getUserMediaFailedCallback')
  template = FillInParameter('FUZZ_FAIL_CALLBACK', fail_callback, template)

  before_call = 'location.reload();' if random.random() < 0.1 else ''
  template = FillInParameter('BEFORE_GET_USER_MEDIA_CALL', before_call,
                             template)

  after_call = 'location.reload();' if random.random() < 0.3 else ''
  template = FillInParameter('AFTER_GET_USER_MEDIA_CALL', after_call,
                             template)

  return template
