/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include <string>

#include "CC_Common.h"
#include "ECC_Types.h"

namespace CSF
{
	DECLARE_NS_PTR_VECTOR(PhoneDetails);
	class ECC_API PhoneDetails
	{
	public:
                NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PhoneDetails)
		virtual ~PhoneDetails() {}
		/**
		 * Get the device name (the CUCM device name) and the free text description.
		 */
		virtual std::string getName() const = 0;
		virtual std::string getDescription() const = 0;

		/**
		 * Get the model number (the internal CUCM number, not the number printed on the phone)
		 * and the corresponding description (which normally does include the number printed on the phone).
		 * Returns -1, "" if unknown
		 */
		virtual int getModel() const = 0;
		virtual std::string getModelDescription() const = 0;

		virtual bool isSoftPhone() = 0;

		/**
		 * List the known directory numbers of lines associated with the device.
		 * Empty list if unknown.
		 */
		virtual std::vector<std::string> getLineDNs() const = 0;

		/**
		 * Current status of the device, if known.
		 */
		virtual ServiceStateType::ServiceState getServiceState() const = 0;

		/**
		 * TFTP config of device, and freshness of the config.
		 */
		virtual std::string getConfig() const = 0;

	protected:
		PhoneDetails() {}

	private:
		PhoneDetails(const PhoneDetails&);
		PhoneDetails& operator=(const PhoneDetails&);
	};
};
