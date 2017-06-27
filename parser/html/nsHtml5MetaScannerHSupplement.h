/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
  using Encoding = mozilla::Encoding;
private:
  const Encoding* mEncoding;
  inline int32_t read()
  {
    return readable->read();
  }
public:
  const Encoding* sniff(nsHtml5ByteReadable* bytes);
