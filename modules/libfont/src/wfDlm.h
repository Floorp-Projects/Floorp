/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


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

