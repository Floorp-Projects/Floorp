/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

protected:
  nsHtml5UTF16Buffer(PRUnichar* aBuffer, PRInt32 aEnd);
  ~nsHtml5UTF16Buffer();

  /**
   * For working around the privacy of |buffer| in the generated code.
   */
  void DeleteBuffer();

  /**
   * For working around the privacy of |buffer| in the generated code.
   */
  void Swap(nsHtml5UTF16Buffer* aOther);
