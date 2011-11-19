/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
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

#ifndef __mozilla_widget_GfxInfoCollector_h__
#define __mozilla_widget_GfxInfoCollector_h__

#include "jsapi.h"

namespace mozilla {
namespace widget {


/* this is handy wrapper around JSAPI to make it more pleasant to use.
 * We collect the JSAPI errors and so that callers don't need to */
class InfoObject
{
  friend class GfxInfoBase;

  public:
  void DefineProperty(const char *name, int value)
  {
    if (!mOk)
      return;

    mOk = JS_DefineProperty(mCx, mObj, name, INT_TO_JSVAL(value), NULL, NULL, JSPROP_ENUMERATE);
  }

  void DefineProperty(const char *name, nsAString &value)
  {
    if (!mOk)
      return;

    const nsPromiseFlatString &flat = PromiseFlatString(value);
    JSString *string = JS_NewUCStringCopyN(mCx, static_cast<const jschar*>(flat.get()), flat.Length());
    if (!string)
      mOk = JS_FALSE;

    if (!mOk)
      return;

    mOk = JS_DefineProperty(mCx, mObj, name, STRING_TO_JSVAL(string), NULL, NULL, JSPROP_ENUMERATE);
  }

  void DefineProperty(const char *name, const char *value)
  { 
    nsAutoString string = NS_ConvertASCIItoUTF16(value);
    DefineProperty(name, string); 
  }

  private:
  // We need to ensure that this object lives on the stack so that GC sees it properly
  InfoObject(JSContext *aCx) : mCx(aCx), mOk(JS_TRUE)
  {
    mObj = JS_NewObject(mCx, NULL, NULL, NULL);
    if (!mObj)
      mOk = JS_FALSE;
  }
  InfoObject(InfoObject&);

  JSContext *mCx;
  JSObject *mObj;
  JSBool mOk;
};

/*

   Here's an example usage:

   class Foo {
   Foo::Foo() : mInfoCollector(this, &Foo::GetAweseomeness) {}

   void GetAwesomeness(InfoObject &obj) {
     obj.DefineProperty("awesome", mAwesome);
   }

   int mAwesome;

   GfxInfoCollector<Foo> mInfoCollector;
   }

   This will define a property on the object
   returned from calling getInfo() on a
   GfxInfo object. e.g.

       gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);
       info = gfxInfo.getInfo();
       if (info.awesome)
          alert(info.awesome);

*/

class GfxInfoCollectorBase
{
  public:
  GfxInfoCollectorBase();
  virtual void GetInfo(InfoObject &obj) = 0;
  virtual ~GfxInfoCollectorBase();
};

template<class T>
class GfxInfoCollector : public GfxInfoCollectorBase
{
  public:
  GfxInfoCollector(T* aPointer, void (T::*aFunc)(InfoObject &obj)) : mPointer(aPointer), mFunc(aFunc) {
  }
  virtual void GetInfo(InfoObject &obj) {
    (mPointer->*mFunc)(obj);
  }

  protected:
  T* mPointer;
  void (T::*mFunc)(InfoObject &obj);

};

}
}

#endif
