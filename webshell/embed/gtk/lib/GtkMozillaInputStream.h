/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of the Original Code is Alexander. Portions
 * created by Alexander Larsson are Copyright (C) 1999
 * Alexander Larsson. All Rights Reserved. 
 */
#ifndef GTKMOZILLAINPUTSTREAM_H
#define GTKMOZILLAINPUTSTREAM_H

#include "nsIInputStream.h"

class GtkMozillaInputStream : public nsIInputStream {
public:
  GtkMozillaInputStream(void);
  ~GtkMozillaInputStream(void);
  
  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsIInputStream interface
  NS_DECL_NSIINPUTSTREAM
  
  // nsIBaseStream interface
  NS_DECL_NSIBASESTREAM

  // Specific interface
  NS_IMETHOD Fill(const char *buffer, PRUint32 aCount);
  NS_IMETHOD FillResult(PRUint32 *aReadCount);
  
protected:
  PRUint32 mReadCount;
  PRUint32 mCount;
  const char *mBuffer;
};


#endif /* GTKMOZILLAINPUTSTREAM_H */
