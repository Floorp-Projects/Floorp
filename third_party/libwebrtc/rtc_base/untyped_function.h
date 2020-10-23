/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_UNTYPED_FUNCTION_H_
#define RTC_BASE_UNTYPED_FUNCTION_H_

#include <memory>
#include <type_traits>
#include <utility>

#include "rtc_base/system/assume.h"

namespace webrtc {
namespace webrtc_function_impl {

using FunVoid = void();

union VoidUnion {
  void* void_ptr;
  FunVoid* fun_ptr;
  typename std::aligned_storage<16>::type inline_storage;
};

template <typename T>
struct CallHelpers;
template <typename RetT, typename... ArgT>
struct CallHelpers<RetT(ArgT...)> {
  // Return type of the three helpers below.
  using return_type = RetT;
  // Complete function type of the three helpers below.
  using function_type = RetT(VoidUnion*, ArgT...);
  // Helper for calling the `void_ptr` case of VoidUnion.
  template <typename F>
  static RetT CallVoidPtr(VoidUnion* vu, ArgT... args) {
    return (*static_cast<F*>(vu->void_ptr))(std::forward<ArgT>(args)...);
  }
  // Helper for calling the `fun_ptr` case of VoidUnion.
  static RetT CallFunPtr(VoidUnion* vu, ArgT... args) {
    return (reinterpret_cast<RetT (*)(ArgT...)>(vu->fun_ptr))(
        std::forward<ArgT>(args)...);
  }
  // Helper for calling the `inline_storage` case of VoidUnion.
  template <typename F>
  static RetT CallInlineStorage(VoidUnion* vu, ArgT... args) {
    return (*reinterpret_cast<F*>(&vu->inline_storage))(
        std::forward<ArgT>(args)...);
  }
};

}  // namespace webrtc_function_impl

// A class that holds (and owns) any callable. The same function call signature
// must be provided when constructing and calling the object.
//
// The point of not having the call signature as a class template parameter is
// to have one single concrete type for all signatures; this reduces binary
// size.
class UntypedFunction final {
 public:
  // Create function for lambdas and other callables; it accepts every type of
  // argument except those noted in its enable_if call.
  template <
      typename Signature,
      typename F,
      typename std::enable_if<
          // Not for function pointers; we have another overload for that below.
          !std::is_function<typename std::remove_pointer<
              typename std::remove_reference<F>::type>::type>::value &&

          // Not for nullptr; we have another overload for that below.
          !std::is_same<std::nullptr_t,
                        typename std::remove_cv<F>::type>::value &&

          // Not for UntypedFunction objects; we have another overload for that.
          !std::is_same<UntypedFunction,
                        typename std::remove_cv<typename std::remove_reference<
                            F>::type>::type>::value>::type* = nullptr>
  static UntypedFunction Create(F&& f) {
    using F_deref = typename std::remove_reference<F>::type;
    // TODO(C++17): Use `constexpr if` here. The compiler appears to do the
    // right thing anyway w.r.t. resolving the branch statically and
    // eliminating dead code, but it would be good for readability.
    if (std::is_trivially_move_constructible<F_deref>::value &&
        std::is_trivially_destructible<F_deref>::value &&
        sizeof(F_deref) <=
            sizeof(webrtc_function_impl::VoidUnion::inline_storage)) {
      // The callable is trivial and small enough, so we just store its bytes
      // in the inline storage.
      webrtc_function_impl::VoidUnion vu;
      new (&vu.inline_storage) F_deref(std::forward<F>(f));
      return UntypedFunction(
          vu,
          reinterpret_cast<webrtc_function_impl::FunVoid*>(
              webrtc_function_impl::CallHelpers<
                  Signature>::template CallInlineStorage<F_deref>),
          nullptr);
    } else {
      // The callable is either nontrivial or too large, so we can't keep it
      // in the inline storage; use the heap instead.
      webrtc_function_impl::VoidUnion vu;
      vu.void_ptr = new F_deref(std::forward<F>(f));
      return UntypedFunction(
          vu,
          reinterpret_cast<webrtc_function_impl::FunVoid*>(
              webrtc_function_impl::CallHelpers<
                  Signature>::template CallVoidPtr<F_deref>),
          static_cast<void (*)(webrtc_function_impl::VoidUnion*)>(
              [](webrtc_function_impl::VoidUnion* vu) {
                // Assuming that this pointer isn't null allows the
                // compiler to eliminate a null check in the (inlined)
                // delete operation.
                RTC_ASSUME(vu->void_ptr != nullptr);
                delete reinterpret_cast<F_deref*>(vu->void_ptr);
              }));
    }
  }

  // Create function that accepts function pointers. If the argument is null,
  // the result is an empty UntypedFunction.
  template <typename Signature>
  static UntypedFunction Create(Signature* f) {
    webrtc_function_impl::VoidUnion vu;
    vu.fun_ptr = reinterpret_cast<webrtc_function_impl::FunVoid*>(f);
    return UntypedFunction(
        vu,
        f ? reinterpret_cast<webrtc_function_impl::FunVoid*>(
                webrtc_function_impl::CallHelpers<Signature>::CallFunPtr)
          : nullptr,
        nullptr);
  }

  // Default constructor. Creates an empty UntypedFunction.
  UntypedFunction() : call_(nullptr), delete_(nullptr) {}

  // Nullptr constructor and assignment. Creates an empty UntypedFunction.
  UntypedFunction(std::nullptr_t)  // NOLINT(runtime/explicit)
      : call_(nullptr), delete_(nullptr) {}
  UntypedFunction& operator=(std::nullptr_t) {
    call_ = nullptr;
    if (delete_) {
      delete_(&f_);
      delete_ = nullptr;
    }
    return *this;
  }

  // Not copyable.
  UntypedFunction(const UntypedFunction&) = delete;
  UntypedFunction& operator=(const UntypedFunction&) = delete;

  // Move construction and assignment.
  UntypedFunction(UntypedFunction&& other)
      : f_(other.f_), call_(other.call_), delete_(other.delete_) {
    other.delete_ = nullptr;
  }
  UntypedFunction& operator=(UntypedFunction&& other) {
    if (delete_) {
      delete_(&f_);
    }
    f_ = other.f_;
    call_ = other.call_;
    delete_ = other.delete_;
    other.delete_ = nullptr;
    return *this;
  }

  ~UntypedFunction() {
    if (delete_) {
      delete_(&f_);
    }
  }

  friend void swap(UntypedFunction& a, UntypedFunction& b) {
    using std::swap;
    swap(a.f_, b.f_);
    swap(a.call_, b.call_);
    swap(a.delete_, b.delete_);
  }

  // Returns true if we have a function, false if we don't (i.e., we're null).
  explicit operator bool() const { return call_ != nullptr; }

  template <typename Signature, typename... ArgT>
  typename webrtc_function_impl::CallHelpers<Signature>::return_type Call(
      ArgT&&... args) {
    return reinterpret_cast<
        typename webrtc_function_impl::CallHelpers<Signature>::function_type*>(
        call_)(&f_, std::forward<ArgT>(args)...);
  }

  // Returns true iff we don't need to call a destructor. This is guaranteed
  // to hold for a moved-from object.
  bool IsTriviallyDestructible() { return delete_ == nullptr; }

 private:
  UntypedFunction(webrtc_function_impl::VoidUnion f,
                  webrtc_function_impl::FunVoid* call,
                  void (*del)(webrtc_function_impl::VoidUnion*))
      : f_(f), call_(call), delete_(del) {}

  // The callable thing, or a pointer to it.
  webrtc_function_impl::VoidUnion f_;

  // Pointer to a dispatch function that knows the type of the callable thing
  // that's stored in f_, and how to call it. An UntypedFunction object is empty
  // (null) iff call_ is null.
  webrtc_function_impl::FunVoid* call_;

  // Pointer to a function that knows how to delete the callable thing that's
  // stored in f_. Null if `f_` is trivially deletable.
  void (*delete_)(webrtc_function_impl::VoidUnion*);
};

}  // namespace webrtc

#endif  // RTC_BASE_UNTYPED_FUNCTION_H_
