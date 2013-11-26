/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
private:
  nsCString mCharset;
  inline int32_t read() {
    return readable->read();
  }
public:
  void sniff(nsHtml5ByteReadable* bytes, nsACString& charset);
