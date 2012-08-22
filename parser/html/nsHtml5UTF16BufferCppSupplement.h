/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

nsHtml5UTF16Buffer::nsHtml5UTF16Buffer(PRUnichar* aBuffer, int32_t aEnd)
  : buffer(aBuffer)
  , start(0)
  , end(aEnd)
{
  MOZ_COUNT_CTOR(nsHtml5UTF16Buffer);
}

nsHtml5UTF16Buffer::~nsHtml5UTF16Buffer()
{
  MOZ_COUNT_DTOR(nsHtml5UTF16Buffer);
}

void
nsHtml5UTF16Buffer::DeleteBuffer()
{
  delete[] buffer;
}

void
nsHtml5UTF16Buffer::Swap(nsHtml5UTF16Buffer* aOther)
{
  PRUnichar* tempBuffer = buffer;
  int32_t tempStart = start;
  int32_t tempEnd = end;
  buffer = aOther->buffer;
  start = aOther->start;
  end = aOther->end;
  aOther->buffer = tempBuffer;
  aOther->start = tempStart;
  aOther->end = tempEnd;
}
