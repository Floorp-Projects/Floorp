/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

// This file holds our vocabulary definition for the helper
// app data source. The helper app data source contains user specified helper
// application override information.


#define NC_RDF_MIMETYPES				NC_NAMESPACE_URI"MIME-Types"
// a mime type has the following properties...
#define NC_RDF_MIMETYPE 				NC_NAMESPACE_URI"MIME-Type"
#define NC_RDF_DESCRIPTION			NC_NAMESPACE_URI"description"
#define NC_RDF_VALUE					  NC_NAMESPACE_URI"value"
#define NC_RDF_EDITABLE   			NC_NAMESPACE_URI"editable"
#define NC_RDF_LARGEICON				NC_NAMESPACE_URI"largeIcon"
#define NC_RDF_SMALLICON				NC_NAMESPACE_URI"smallIcon"
#define NC_RDF_HANDLER				  NC_NAMESPACE_URI"handler"
#define NC_RDF_FILEEXTENSIONS	  NC_NAMESPACE_URI"fileExtensions"
#define NC_RDF_CHILD            NC_NAMESPACE_URI"child"
#define NC_RDF_ROOT 			      "NC:HelperAppRoot"
#define NC_CONTENT_NODE_PREFIX  "urn:mimetype:"
#define NC_CONTENT_NODE_HANDLER_PREFIX "urn:mimetype:handler:"
#define NC_CONTENT_NODE_EXTERNALAPP_PREFIX "urn:mimetype:externalApplication:"

// File Extensions have file extension properties....
#define NC_RDF_FILEEXTENSION    NC_NAMESPACE_URI"fileExtension"

// handler properties
#define NC_RDF_SAVETODISK				    NC_NAMESPACE_URI"saveToDisk"
#define NC_RDF_HANDLEINTERNAL       NC_NAMESPACE_URI"handleInternal"
#define NC_RDF_ALWAYSASK            NC_NAMESPACE_URI"alwaysAsk"
#define NC_RDF_EXTERNALAPPLICATION  NC_NAMESPACE_URI"externalApplication"

// external applications properties....
#define NC_RDF_PRETTYNAME 			    NC_NAMESPACE_URI"prettyName"
#define NC_RDF_PATH 			          NC_NAMESPACE_URI"path"

