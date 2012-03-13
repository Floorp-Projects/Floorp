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
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
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

#pragma once

#include "CC_Common.h"

extern "C"
{
#include "ccapi_types.h"
}

namespace CSF
{

    class ECC_API CC_Device
    {
    protected:
        CC_Device() {}

    public:
        virtual ~CC_Device() {};

        virtual std::string toString() = 0;

        virtual CC_DeviceInfoPtr getDeviceInfo () = 0;

        /**
           Create a call on the device. Line selection is on the first available line.
           Lines that have their MNC reached will be skipped. If you have a specific line
           you want to make a call on (assuming the device has more than available) then
           you should use CC_Line::createCall() to do that.

           @return CC_CallPtr - the created call object wrapped in a smart_ptr.
         */
        virtual CC_CallPtr createCall() = 0;

        virtual void enableVideo(bool enable) = 0;
        virtual void enableCamera(bool enable) = 0;    
		virtual void setDigestNamePasswd (char *name, char *pw) = 0;

    private:
		// Cannot copy - clients should be passing the pointer not the object.
		CC_Device& operator=(const CC_Device& rhs);
		CC_Device(const CC_Device&);
    };
};
