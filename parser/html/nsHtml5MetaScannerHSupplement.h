/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
private:
  nsCOMPtr<nsIUnicodeDecoder>  mUnicodeDecoder;
  nsCString mCharset;
  inline PRInt32 read() {
    return readable->read();
  }
public:
  void sniff(nsHtml5ByteReadable* bytes, nsIUnicodeDecoder** decoder, nsACString& charset);
