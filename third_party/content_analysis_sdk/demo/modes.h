/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

// #define AGENT_MODE(name) to do what you want and then #include this file

AGENT_MODE(largeResponse)
AGENT_MODE(invalidUtf8StringStartByteIsContinuationByte)
AGENT_MODE(invalidUtf8StringEndsInMiddleOfMultibyteSequence)
AGENT_MODE(invalidUtf8StringOverlongEncoding)
AGENT_MODE(invalidUtf8StringMultibyteSequenceTooShort)
AGENT_MODE(invalidUtf8StringDecodesToInvalidCodePoint)
AGENT_MODE(stringWithEmbeddedNull)
AGENT_MODE(zeroResults)
AGENT_MODE(resultWithInvalidStatus)
AGENT_MODE(messageTruncatedInMiddleOfString)
AGENT_MODE(messageWithInvalidWireType)
AGENT_MODE(messageWithUnusedFieldNumber)
AGENT_MODE(messageWithWrongStringWireType)
AGENT_MODE(messageWithZeroTag)
AGENT_MODE(messageWithZeroFieldButNonzeroWireType)
AGENT_MODE(messageWithGroupEnd)
AGENT_MODE(messageTruncatedInMiddleOfVarint)
AGENT_MODE(messageTruncatedInMiddleOfTag)
