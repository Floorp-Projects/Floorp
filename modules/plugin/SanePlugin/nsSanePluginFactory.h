/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * Contributor(s): 
 *   Rusty Lynch <rusty.lynch@intel.com>
 */
/*
 * Declares the SANE plugin factory class.
 */

#ifndef __NS_SANE_PLUGIN_FACTORY_H__
#define __NS_SANE_PLUGIN_FACTORY_H__

class nsSanePluginFactoryImpl : public nsIPlugin
{
public:
    nsSanePluginFactoryImpl(const nsCID &aClass, const char* className,
                            const char* contractID);

    // nsISupports methods
    NS_DECL_ISUPPORTS ;

    // nsIFactory methods
    NS_IMETHOD CreateInstance(nsISupports *aOuter,
                              const nsIID &aIID,
                              void **aResult);

    NS_IMETHOD LockFactory(PRBool aLock);
    NS_IMETHOD Initialize(void);
    NS_IMETHOD Shutdown(void);
    NS_IMETHOD GetMIMEDescription(const char* *result);
    NS_IMETHOD GetValue(nsPluginVariable variable, void *value);
    NS_IMETHOD CreatePluginInstance(nsISupports *aOuter, REFNSIID aIID, 
                                    const char* aPluginMIMEType,
                                    void **aResult);

protected:
    virtual ~nsSanePluginFactoryImpl();

  nsCID       mClassID;
  const char* mClassName;
  const char* mContractID;

};

#endif // __NS_SANE_PLUGIN_FACTORY_H__





