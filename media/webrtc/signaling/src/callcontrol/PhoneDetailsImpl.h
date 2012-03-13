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

#include "PhoneDetails.h"

namespace CSF
{
	DECLARE_PTR(PhoneDetailsImpl);
	class PhoneDetailsImpl: public PhoneDetails
	{
	public:
		virtual std::string getName() const {return name; }
		virtual std::string getDescription() const {return description; }
		virtual int getModel() const {return model;}
		virtual std::string getModelDescription() const {return modelDescription; }
		virtual bool isSoftPhone();
		virtual std::vector<std::string> getLineDNs() const {return lineDNs; }
		virtual ServiceStateType::ServiceState getServiceState() const { return state; }
		virtual std::string getConfig() const { return config; }

	public:
		PhoneDetailsImpl();
		virtual ~PhoneDetailsImpl();

		virtual void setName(const std::string& name);
		virtual void setDescription(const std::string& description);
		// Note that setting model and model description are mutually exclusive.
		virtual void setModel(int model);
		virtual void setModelDescription(const std::string& description);
		virtual void setLineDNs(const std::vector<std::string> & lineDNs);
		virtual void setServiceState(ServiceStateType::ServiceState state);
		virtual void setConfig(const std::string& config);

	private:
		std::string name;
		std::string description;
		int model;
		std::string modelDescription;
		std::vector<std::string> lineDNs;
		ServiceStateType::ServiceState state;
		std::string config;

	};
}
