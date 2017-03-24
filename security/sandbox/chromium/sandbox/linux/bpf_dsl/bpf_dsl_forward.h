// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_BPF_DSL_BPF_DSL_FORWARD_H_
#define SANDBOX_LINUX_BPF_DSL_BPF_DSL_FORWARD_H_

#include "base/memory/ref_counted.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {
namespace bpf_dsl {

// The bpf_dsl_forward.h header provides forward declarations for the
// types defined in bpf_dsl.h. It's intended for use in user headers
// that need to reference bpf_dsl types, but don't require definitions.

namespace internal {
class ResultExprImpl;
class BoolExprImpl;
}

typedef scoped_refptr<const internal::ResultExprImpl> ResultExpr;
typedef scoped_refptr<const internal::BoolExprImpl> BoolExpr;

template <typename T>
class Arg;

class Elser;

template <typename T>
class Caser;

}  // namespace bpf_dsl
}  // namespace sandbox

extern template class SANDBOX_EXPORT
    scoped_refptr<const sandbox::bpf_dsl::internal::BoolExprImpl>;
extern template class SANDBOX_EXPORT
    scoped_refptr<const sandbox::bpf_dsl::internal::ResultExprImpl>;

#endif  // SANDBOX_LINUX_BPF_DSL_BPF_DSL_FORWARD_H_
