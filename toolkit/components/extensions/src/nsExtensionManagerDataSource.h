/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Extension Manager.
 *
 * The Initial Developer of the Original Code is Ben Goodger.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Ben Goodger <ben@bengoodger.com>
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

#ifndef extensionmanagerdatasource___h___
#define extensionmanagerdatasource___h___

#include "nsCOMPtr.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFCompositeDataSource.h"

class nsIFile;

class nsExtensionManagerDataSource : public nsIRDFDataSource
{
public:
  NS_DECL_NSIRDFDATASOURCE
  NS_DECL_ISUPPORTS

  nsExtensionManagerDataSource();
  virtual ~nsExtensionManagerDataSource();

public:
  nsresult InstallExtension(nsIRDFDataSource* aSourceDataSource,
                            PRBool aProfile);

  nsresult SetToBeEnabled(const PRUnichar* aExtensionID);
  nsresult SetToBeDisabled(const PRUnichar* aExtensionID);
  nsresult SetToBeUninstalled(const PRUnichar* aExtensionID);

  nsresult LoadExtensions(PRBool aProfile);

protected:
  nsresult SetExtensionProperty(const PRUnichar* aExtensionID, 
                                nsIRDFResource* aPropertyArc, 
                                nsIRDFNode* aPropertyValue);
  void      InitLexicalResources();

private:
  nsCOMPtr<nsIRDFCompositeDataSource> mComposite; // A convenience to handle 
                                                  // precedence between the 
                                                  // profile and app extensions
                                                  // datasources.
  nsCOMPtr<nsIRDFDataSource> mProfileExtensions;
  nsCOMPtr<nsIRDFDataSource> mAppExtensions;
};
 
#endif

