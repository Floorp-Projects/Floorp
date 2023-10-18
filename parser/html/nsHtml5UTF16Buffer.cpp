/*
 * Copyright (c) 2008-2010 Mozilla Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * THIS IS A GENERATED FILE. PLEASE DO NOT EDIT.
 * Please edit UTF16Buffer.java instead and regenerate.
 */

#define nsHtml5UTF16Buffer_cpp__

#include "nsHtml5AttributeName.h"
#include "nsHtml5ElementName.h"
#include "nsHtml5Tokenizer.h"
#include "nsHtml5TreeBuilder.h"
#include "nsHtml5StackNode.h"
#include "nsHtml5StateSnapshot.h"
#include "nsHtml5Portability.h"

#include "nsHtml5UTF16Buffer.h"

int32_t nsHtml5UTF16Buffer::getStart() { return start; }

void nsHtml5UTF16Buffer::setStart(int32_t start) { this->start = start; }

char16_t* nsHtml5UTF16Buffer::getBuffer() { return buffer; }

int32_t nsHtml5UTF16Buffer::getEnd() { return end; }

bool nsHtml5UTF16Buffer::hasMore() { return start < end; }

int32_t nsHtml5UTF16Buffer::getLength() { return end - start; }

void nsHtml5UTF16Buffer::adjust(bool lastWasCR) {
  if (lastWasCR && buffer[start] == '\n') {
    start++;
  }
}

void nsHtml5UTF16Buffer::setEnd(int32_t end) { this->end = end; }

void nsHtml5UTF16Buffer::initializeStatics() {}

void nsHtml5UTF16Buffer::releaseStatics() {}

#include "nsHtml5UTF16BufferCppSupplement.h"
