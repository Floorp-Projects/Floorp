/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUnicharInputStream_h__
#define nsUnicharInputStream_h__

#include "nsISimpleUnicharStreamFactory.h"
#include "nsIUnicharInputStream.h"
#include "nsIFactory.h"

// {428DCA6F-1A0F-4cda-B516-0D5244745A6A}
#define NS_SIMPLE_UNICHAR_STREAM_FACTORY_CID \
{ 0x428dca6f, 0x1a0f, 0x4cda, { 0xb5, 0x16, 0xd, 0x52, 0x44, 0x74, 0x5a, 0x6a } }

class nsSimpleUnicharStreamFactory :
  public nsIFactory, 
  private nsISimpleUnicharStreamFactory
{
public:
  nsSimpleUnicharStreamFactory() {}
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIFACTORY
  NS_DECL_NSISIMPLEUNICHARSTREAMFACTORY

  static nsSimpleUnicharStreamFactory* GetInstance();

private:
  static const nsSimpleUnicharStreamFactory kInstance;
};

#endif // nsUnicharInputStream_h__
