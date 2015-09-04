/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_devtools_ZeroCopyNSIOutputStream__
#define mozilla_devtools_ZeroCopyNSIOutputStream__

#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/stubs/common.h>

#include "nsCOMPtr.h"
#include "nsIOutputStream.h"

namespace mozilla {
namespace devtools {

// A `google::protobuf::io::ZeroCopyOutputStream` implementation that uses an
// `nsIOutputStream` under the covers.
//
// This class will automatically write and flush its data to the
// `nsIOutputStream` in its destructor, but if you care whether that call
// succeeds or fails, then you should call the `flush` method yourself. Errors
// will be logged, however.
class MOZ_STACK_CLASS ZeroCopyNSIOutputStream
  : public ::google::protobuf::io::ZeroCopyOutputStream
{
  static const int BUFFER_SIZE = 8192;

  // The nsIOutputStream we are streaming to.
  nsCOMPtr<nsIOutputStream>& out;

  // The buffer we write data to before passing it to the output stream.
  char buffer[BUFFER_SIZE];

  // The status of writing to the underlying output stream.
  nsresult result_;

  // The number of bytes in the buffer that have been used thus far.
  int amountUsed;

  // Excluding the amount of the buffer currently used (which hasn't been
  // written and flushed yet), this is the number of bytes written to the output
  // stream.
  int64_t writtenCount;

  // Write the internal buffer to the output stream and flush it.
  nsresult writeBuffer();

public:
  explicit ZeroCopyNSIOutputStream(nsCOMPtr<nsIOutputStream>& out);

  nsresult flush() { return writeBuffer(); }

  // Return true if writing to the underlying output stream ever failed.
  bool failed() const { return NS_FAILED(result_); }

  nsresult result() const { return result_; }

  // ZeroCopyOutputStream Interface
  virtual ~ZeroCopyNSIOutputStream() override;
  virtual bool Next(void** data, int* size) override;
  virtual void BackUp(int count) override;
  virtual ::google::protobuf::int64 ByteCount() const override;
};

} // namespace devtools
} // namespace mozilla

#endif // mozilla_devtools_ZeroCopyNSIOutputStream__
