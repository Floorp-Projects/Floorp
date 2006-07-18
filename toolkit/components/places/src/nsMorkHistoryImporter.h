/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Places.
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Ryner <bryner@brianryner.com> (original author)
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

#ifndef nsMorkHistoryImporter_h_
#define nsMorkHistoryImporter_h_

#include "nsINavHistoryService.h"
#include "nsMorkReader.h"

// The nsMorkHistoryImporter object parses a Mork-format history file and
// adds the history items to the NavHistoryService.  It is invoked the first
// time the history service is created for a given profile, if a Mork history
// (history.dat) file exists.

class nsMorkHistoryImporter : public nsIMorkHistoryImporter
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMORKHISTORYIMPORTER

private:
  // Enumerator callback to build up a list of columns
  static PLDHashOperator PR_CALLBACK
  EnumerateColumnsCB(const nsACString &aColumnID,
                     nsCString aName,
                     void *aData);

  // Enumerator callback to add a single row to the NavHistory.
  static PLDHashOperator PR_CALLBACK
  AddToHistoryCB(const nsACString &aRowID,
                 const nsMorkReader::StringMap *aMap,
                 void *aData);
};

#endif // nsMorkHistoryImporter_h_
