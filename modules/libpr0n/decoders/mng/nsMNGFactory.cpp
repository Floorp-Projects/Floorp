/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Tim Rowley.
 * Portions created by Tim Rowley are 
 * Copyright (C) 2001 Tim Rowley.  Rights Reserved.
 * 
 * Contributor(s): 
 *   Tim Rowley <tor@cs.brown.edu>
 */

#include "nsIGenericFactory.h"
#include "nsIModule.h"
#include "nsMNGDecoder.h"
#include "imgContainerMNG.h"

// objects that just require generic constructors

NS_GENERIC_FACTORY_CONSTRUCTOR(nsMNGDecoder)
NS_GENERIC_FACTORY_CONSTRUCTOR(imgContainerMNG)

static const nsModuleComponentInfo components[] =
{
  { "MNG decoder",
    NS_MNGDECODER_CID,
    "@mozilla.org/image/decoder;2?type=video/x-mng",
    nsMNGDecoderConstructor, },
  { "JNG decoder",
    NS_MNGDECODER_CID,
    "@mozilla.org/image/decoder;2?type=image/x-jng",
    nsMNGDecoderConstructor, },
  { "MNG/JNG image container",
    NS_MNGCONTAINER_CID,
    "@mozilla.org/image/container;1?type=image/x-mng",
    imgContainerMNGConstructor, },
};

NS_IMPL_NSGETMODULE(nsMNGDecoderModule, components)
