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
