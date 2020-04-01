/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ArrayAlgorithm_h___
#define ArrayAlgorithm_h___

#include "nsTArray.h"

#include "mozilla/Algorithm.h"

namespace mozilla {

// An algorithm similar to TransformAbortOnErr, which creates a new nsTArray
// rather than inserting into an output iterator. The capacity of the result
// array is set to the number of elements that will be inserted if all
// transformations are successful. This variant is fallible, i.e. if setting the
// capacity fails, NS_ERROR_OUT_OF_MEMORY is returned as an error value. This
// variant only works with nsresult errors.
template <
    typename SrcIter, typename Transform,
    typename = std::enable_if_t<std::is_same_v<
        typename detail::TransformTraits<Transform, SrcIter>::result_err_type,
        nsresult>>>
Result<nsTArray<typename detail::TransformTraits<Transform,
                                                 SrcIter>::result_ok_type>,
       nsresult>
TransformIntoNewArrayAbortOnErr(SrcIter aIter, SrcIter aEnd,
                                Transform aTransform, fallible_t) {
  nsTArray<typename detail::TransformTraits<Transform, SrcIter>::result_ok_type>
      res;
  if (!res.SetCapacity(std::distance(aIter, aEnd), fallible)) {
    return Err(NS_ERROR_OUT_OF_MEMORY);
  }

  auto transformRes = TransformAbortOnErr(aIter, aEnd, MakeBackInserter(res),
                                          std::move(aTransform));
  if (NS_WARN_IF(transformRes.isErr())) {
    return Err(transformRes.unwrapErr());
  }

  return res;
}

template <typename SrcRange, typename Transform>
auto TransformIntoNewArrayAbortOnErr(SrcRange& aRange, Transform aTransform,
                                     fallible_t) {
  using std::begin;
  using std::end;
  return TransformIntoNewArrayAbortOnErr(begin(aRange), end(aRange), aTransform,
                                         fallible);
}

// An algorithm similar to std::transform, which creates a new nsTArray
// rather than inserting into an output iterator. The capacity of the result
// array is set to the number of elements that will be inserted. This variant is
// fallible, i.e. if setting the capacity fails, NS_ERROR_OUT_OF_MEMORY is
// returned as an error value. This variant only works with nsresult errors.
template <typename SrcIter, typename Transform>
Result<nsTArray<detail::ArrayElementTransformType<Transform, SrcIter>>,
       nsresult>
TransformIntoNewArray(SrcIter aIter, SrcIter aEnd, Transform aTransform,
                      fallible_t) {
  nsTArray<detail::ArrayElementTransformType<Transform, SrcIter>> res;
  if (!res.SetCapacity(std::distance(aIter, aEnd), fallible)) {
    return Err(NS_ERROR_OUT_OF_MEMORY);
  }

  std::transform(aIter, aEnd, MakeBackInserter(res), std::move(aTransform));

  return res;
}

template <typename SrcRange, typename Transform>
auto TransformIntoNewArray(SrcRange& aRange, Transform aTransform, fallible_t) {
  using std::begin;
  using std::end;
  return TransformIntoNewArray(begin(aRange), end(aRange), aTransform,
                               fallible);
}

// An algorithm similar to std::transform, which creates a new nsTArray
// rather than inserting into an output iterator. The capacity of the result
// array is set to the number of elements that will be inserted. This variant is
// infallible, i.e. if setting the capacity fails, the process is aborted.
template <typename SrcIter, typename Transform>
nsTArray<detail::ArrayElementTransformType<Transform, SrcIter>>
TransformIntoNewArray(SrcIter aIter, SrcIter aEnd, Transform aTransform) {
  nsTArray<detail::ArrayElementTransformType<Transform, SrcIter>> res;
  res.SetCapacity(std::distance(aIter, aEnd));

  std::transform(aIter, aEnd, MakeBackInserter(res), std::move(aTransform));

  return res;
}

template <typename SrcRange, typename Transform>
auto TransformIntoNewArray(SrcRange& aRange, Transform aTransform) {
  using std::begin;
  using std::end;
  return TransformIntoNewArray(begin(aRange), end(aRange), aTransform);
}

}  // namespace mozilla

#endif  // !defined(ArrayAlgorithm_h___)
