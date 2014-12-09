/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PhoneDetailsImpl.h"

#include "csf_common.h"

namespace CSF
{

PhoneDetailsImpl::PhoneDetailsImpl()
: model(-1),
  state(ServiceStateType::eUnknown)
{
}

PhoneDetailsImpl::~PhoneDetailsImpl()
{
}

static const char * _softphoneSupportedModelNames[] = { "Cisco Unified Client Services Framework",
                                                        "Client Services Framework",
                                                        "Client Services Core" };

bool PhoneDetailsImpl::isSoftPhone()
{
	if(model == -1 && modelDescription != "")
	{
		// Evaluate based on model description.
		for(int i = 0; i < (int) csf_countof(_softphoneSupportedModelNames); i++)
		{
			if(modelDescription == _softphoneSupportedModelNames[i])
				return true;
		}
		return false;
	}
	else
	{
	}
	return false;
}

void PhoneDetailsImpl::setName(const std::string& name)
{
	this->name = name;
}
void PhoneDetailsImpl::setDescription(const std::string& description)
{
	this->description = description;
}
// Note that setting model and model description are mutually exclusive.
void PhoneDetailsImpl::setModel(int model)
{
	this->model = model;
	this->modelDescription = "";
}
void PhoneDetailsImpl::setModelDescription(const std::string& description)
{
	this->model = -1;
	this->modelDescription = description;
}
void PhoneDetailsImpl::setLineDNs(const std::vector<std::string> & lineDNs)
{
	this->lineDNs.assign(lineDNs.begin(), lineDNs.end());
}
void PhoneDetailsImpl::setServiceState(ServiceStateType::ServiceState state)
{
	this->state = state;
}
void PhoneDetailsImpl::setConfig(const std::string& config)
{
	this->config = config;
}

}
