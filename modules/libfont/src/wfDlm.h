/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


#ifndef _wfDlm_H_
#define _wfDlm_H_

#include "libfont.h"
#include "nf.h"

#include "Mnffp.h"
#include "Mnfdlm.h"

typedef struct nfdlm *nfdlm_OBJECT_CREATE_PROC(struct JMCException *);

class wfDlm {
protected:
  // 0 : stat on file was never done
  // 1 : stat on file was done and was successful
  // -1: stat on file was done and was unsuccessful
  // -2: file cannot be loaded
  // -3: file is not a Displayer dlm
  int m_state;
  
  char *m_filename;
  PRTime m_modifyTime;
  PRUint32 m_fileSize;

  PRLibrary *m_lib;
  struct nfdlm *m_dlmInterface;

public:
  wfDlm(const char *filename, const char *describeStr = NULL);
  ~wfDlm();
  int finalize(void);
  
  // isEmpty: returns positive if filename is either non-existent or 0 size
  //			else returns 0.
  static int isEmpty(const char *filename);
 
  // returns the current status of the dlm object
  // -ve	: invalid
  // 0, +ve	: valid
  int status(void);
  
  // Describes the status and attributes of the class as a string
  // This string can be stored and later used to recreate the exact state of
  // this class.
  char *describe();
  int reconstruct(const char *describeString);

  const char *filename();
  
  // This tracks if a file has changed since the last time we saw it. The
  // criterion for file changing is
  //		1. file modified time
  //		2. file size
  int isChanged();

  //
  // Sync with the current modified time and size of the file
  int sync();

  // Load/Unload the Displayer dlm to memory
  int load(void);
  int unload(int force = 0);
  FARPROC findSymbol(const char *symbol);

  // Maybe we should move this off to a subclass
  struct nffp *createDisplayerObject(struct nffbp* brokerDisplayerInterface);
};
#endif /* _wfDlm_H_ */

