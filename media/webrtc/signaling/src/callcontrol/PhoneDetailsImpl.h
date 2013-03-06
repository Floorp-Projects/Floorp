/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "PhoneDetails.h"

namespace CSF
{
	DECLARE_NS_PTR(PhoneDetailsImpl);
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
