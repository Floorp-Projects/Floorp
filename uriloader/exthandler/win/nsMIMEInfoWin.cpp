/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott MacGregor <mscott@netscape.com>
 *   Christian Biesinger <cbiesinger@web.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsILocalFile.h"
#include "nsMIMEInfoWin.h"

nsMIMEInfoWin::~nsMIMEInfoWin()
{
}

nsresult
nsMIMEInfoWin::LaunchDefaultWithFile(nsIFile* aFile)
{
  // Launch the file, unless it is an executable.
  nsCOMPtr<nsILocalFile> local(do_QueryInterface(aFile));
  if (!local)
    return NS_ERROR_FAILURE;

  PRBool executable = PR_TRUE;
  local->IsExecutable(&executable);
  if (executable)
    return NS_ERROR_FAILURE;

  return local->Launch();
}

NS_IMETHODIMP
nsMIMEInfoWin::GetHasDefaultHandler(PRBool * _retval)
{
  // We have a default application if we have a description
  // We can ShellExecute anything; however, callers are probably interested if
  // there is really an application associated with this type of file
  *_retval = !mDefaultAppDescription.IsEmpty();
  return NS_OK;
}
