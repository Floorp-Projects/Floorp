/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "logging.h"

// nICEr includes
extern "C" {
#include "nr_api.h"
#include "r_memory.h"
#include "local_addr.h"
}

// Local includes
#include "nricestunaddr.h"

namespace mozilla {

MOZ_MTLOG_MODULE("mtransport")

NrIceStunAddr::NrIceStunAddr()
  : localAddr_(new nr_local_addr)
{
  memset(localAddr_, 0, sizeof(nr_local_addr));
}

NrIceStunAddr::NrIceStunAddr(const nr_local_addr* addr)
  : localAddr_(new nr_local_addr)
{
  nr_local_addr_copy(localAddr_,
                     const_cast<nr_local_addr*>(addr));
}

NrIceStunAddr::NrIceStunAddr(const NrIceStunAddr& rhs)
  : localAddr_(new nr_local_addr)
{
  nr_local_addr_copy(localAddr_,
                     const_cast<nr_local_addr*>(rhs.localAddr_));
}

NrIceStunAddr::~NrIceStunAddr()
{
  delete localAddr_;
}

size_t
NrIceStunAddr::SerializationBufferSize() const
{
  return sizeof(nr_local_addr);
}

nsresult
NrIceStunAddr::Serialize(char* buffer, size_t buffer_size) const
{
  if (buffer_size != sizeof(nr_local_addr)) {
    MOZ_MTLOG(ML_ERROR, "Failed trying to serialize NrIceStunAddr, "
                        "input buffer length (" << buffer_size <<
                        ") does not match required length ("
                        << sizeof(nr_local_addr) << ")");
    MOZ_ASSERT(false, "Failed to serialize NrIceStunAddr, bad buffer size");
    return NS_ERROR_FAILURE;
  }

  nr_local_addr* toAddr = (nr_local_addr*)buffer;
  if (nr_local_addr_copy(toAddr, localAddr_)) {
    MOZ_MTLOG(ML_ERROR, "Failed trying to serialize NrIceStunAddr, "
                        "could not copy nr_local_addr.");
    MOZ_ASSERT(false, "Failed to serialize NrIceStunAddr, nr_local_addr_copy failed");
    return NS_ERROR_FAILURE;
  }

  // don't serialize what will be a bad addr when we deserialize
  toAddr->addr.addr = nullptr;

  return NS_OK;
}

nsresult
NrIceStunAddr::Deserialize(const char* buffer, size_t buffer_size)
{
  if (buffer_size != sizeof(nr_local_addr)) {
    MOZ_MTLOG(ML_ERROR, "Failed trying to deserialize NrIceStunAddr, "
                        "input buffer length (" << buffer_size <<
                        ") does not match required length ("
                        << sizeof(nr_local_addr) << ")");
    MOZ_ASSERT(false, "Failed to deserialize NrIceStunAddr, bad buffer size");
    return NS_ERROR_FAILURE;
  }

  nr_local_addr* from_addr =
      const_cast<nr_local_addr*>((const nr_local_addr*)buffer);

  // At this point, from_addr->addr.addr is invalid (null), but will
  // be fixed by nr_local_addr_copy.
  if (nr_local_addr_copy(localAddr_, from_addr)) {
    MOZ_MTLOG(ML_ERROR, "Failed trying to deserialize NrIceStunAddr, "
                        "could not copy nr_local_addr.");
    MOZ_ASSERT(false, "Failed to deserialize NrIceStunAddr, nr_local_addr_copy failed");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

} // namespace mozilla
