/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#ifndef GUIDS_H
#define GUIDS_H

#define NS_EXTERN_IID(_name) \
	extern const nsIID _name;

// Class IDs
NS_EXTERN_IID(kEventQueueServiceCID);

// Interface IDs
NS_EXTERN_IID(kIEventQueueServiceIID);
NS_EXTERN_IID(kIDocumentViewerIID);
NS_EXTERN_IID(kIDOMDocumentIID);
NS_EXTERN_IID(kIDOMNodeIID);
NS_EXTERN_IID(kIDOMElementIID);
NS_EXTERN_IID(kIWebShellContainerIID);
NS_EXTERN_IID(kIStreamObserverIID);
NS_EXTERN_IID(kISupportsIID);

#ifdef USE_PLUGIN
NS_EXTERN_IID(kIFactoryIID);
NS_EXTERN_IID(kIPluginIID);
NS_EXTERN_IID(kIPluginInstanceIID);
#endif

#endif