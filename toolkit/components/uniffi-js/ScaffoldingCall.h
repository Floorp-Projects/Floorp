/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ScaffoldingCall_h
#define mozilla_ScaffoldingCall_h

#include <tuple>
#include <type_traits>
#include "nsIGlobalObject.h"
#include "nsPrintfCString.h"
#include "mozilla/MozPromise.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/dom/OwnedRustBuffer.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ScaffoldingConverter.h"
#include "mozilla/dom/UniFFIBinding.h"
#include "mozilla/dom/UniFFIRust.h"

namespace mozilla::uniffi {

// Low-level result of calling a scaffolding function
//
// This stores what Rust returned in order to convert it into
// UniFFIScaffoldingCallResult
template <typename ReturnType>
struct RustCallResult {
  ReturnType mReturnValue;
  RustCallStatus mCallStatus = {};
};

template <>
struct RustCallResult<void> {
  RustCallStatus mCallStatus = {};
};

// Does the work required to call a scaffolding function
//
// This class is generic over the type signature of the scaffolding function.
// This seems better than being generic over the functions themselves, since it
// saves space whenever 2 functions share a signature.
template <typename ReturnConverter, typename... ArgConverters>
class ScaffoldingCallHandler {
 public:
  // Pointer to a scaffolding function that can be called by this
  // ScaffoldingConverter
  using ScaffoldingFunc = typename ReturnConverter::RustType (*)(
      typename ArgConverters::RustType..., RustCallStatus*);

  // Perform an async scaffolding call
  static already_AddRefed<dom::Promise> CallAsync(
      ScaffoldingFunc aScaffoldingFunc, const dom::GlobalObject& aGlobal,
      const dom::Sequence<dom::ScaffoldingType>& aArgs,
      const nsLiteralCString& aFuncName, ErrorResult& aError) {
    auto convertResult = ConvertJsArgs(aArgs);
    if (convertResult.isErr()) {
      aError.ThrowUnknownError(aFuncName + convertResult.unwrapErr());
      return nullptr;
    }
    auto convertedArgs = convertResult.unwrap();

    // Create the promise that we return to JS
    nsCOMPtr<nsIGlobalObject> xpcomGlobal =
        do_QueryInterface(aGlobal.GetAsSupports());
    RefPtr<dom::Promise> returnPromise =
        dom::Promise::Create(xpcomGlobal, aError);
    if (aError.Failed()) {
      return nullptr;
    }

    // Create a second promise that gets resolved by a background task that
    // calls the scaffolding function
    RefPtr taskPromise = new typename TaskPromiseType::Private(aFuncName.get());
    nsresult dispatchResult = NS_DispatchBackgroundTask(
        NS_NewRunnableFunction(aFuncName.get(),
                               [args = std::move(convertedArgs), taskPromise,
                                aScaffoldingFunc, aFuncName]() mutable {
                                 auto callResult = CallScaffoldingFunc(
                                     aScaffoldingFunc, std::move(args));
                                 taskPromise->Resolve(std::move(callResult),
                                                      aFuncName.get());
                               }),
        NS_DISPATCH_EVENT_MAY_BLOCK);
    if (NS_FAILED(dispatchResult)) {
      taskPromise->Reject(dispatchResult, aFuncName.get());
    }

    // When the background task promise completes, resolve the JS promise
    taskPromise->Then(
        GetCurrentSerialEventTarget(), aFuncName.get(),
        [xpcomGlobal, returnPromise,
         aFuncName](typename TaskPromiseType::ResolveOrRejectValue&& aResult) {
          if (!aResult.IsResolve()) {
            returnPromise->MaybeRejectWithUnknownError(aFuncName);
            return;
          }

          dom::AutoEntryScript aes(xpcomGlobal, aFuncName.get());
          dom::RootedDictionary<dom::UniFFIScaffoldingCallResult> returnValue(
              aes.cx());

          ReturnResult(aes.cx(), aResult.ResolveValue(), returnValue,
                       aFuncName);
          returnPromise->MaybeResolve(returnValue);
        });

    // Return the JS promise, using forget() to convert it to already_AddRefed
    return returnPromise.forget();
  }

  // Perform an sync scaffolding call
  //
  // aFuncName should be a literal C string
  static void CallSync(
      ScaffoldingFunc aScaffoldingFunc, const dom::GlobalObject& aGlobal,
      const dom::Sequence<dom::ScaffoldingType>& aArgs,
      dom::RootedDictionary<dom::UniFFIScaffoldingCallResult>& aReturnValue,
      const nsLiteralCString& aFuncName, ErrorResult& aError) {
    auto convertResult = ConvertJsArgs(aArgs);
    if (convertResult.isErr()) {
      aError.ThrowUnknownError(aFuncName + convertResult.unwrapErr());
      return;
    }

    auto callResult = CallScaffoldingFunc(aScaffoldingFunc,
                                          std::move(convertResult.unwrap()));

    ReturnResult(aGlobal.Context(), callResult, aReturnValue, aFuncName);
  }

 private:
  using RustArgs = std::tuple<typename ArgConverters::RustType...>;
  using IntermediateArgs =
      std::tuple<typename ArgConverters::IntermediateType...>;
  using CallResult = RustCallResult<typename ReturnConverter::RustType>;
  using TaskPromiseType = MozPromise<CallResult, nsresult, true>;

  template <size_t I>
  using NthArgConverter =
      typename std::tuple_element<I, std::tuple<ArgConverters...>>::type;

  // Convert arguments from JS
  //
  // This should be called in the main thread
  static Result<IntermediateArgs, nsCString> ConvertJsArgs(
      const dom::Sequence<dom::ScaffoldingType>& aArgs) {
    IntermediateArgs convertedArgs;
    if (aArgs.Length() != std::tuple_size_v<IntermediateArgs>) {
      return mozilla::Err("Wrong argument count"_ns);
    }
    auto result = PrepareArgsHelper<0>(aArgs, convertedArgs);
    return result.map([&](auto _) { return std::move(convertedArgs); });
  }

  // Helper function for PrepareArgs that uses c++ magic to help with iteration
  template <size_t I = 0>
  static Result<mozilla::Ok, nsCString> PrepareArgsHelper(
      const dom::Sequence<dom::ScaffoldingType>& aArgs,
      IntermediateArgs& aConvertedArgs) {
    if constexpr (I >= sizeof...(ArgConverters)) {
      // Iteration complete
      return mozilla::Ok();
    } else {
      // Single iteration step
      auto result = NthArgConverter<I>::FromJs(aArgs[I]);
      if (result.isOk()) {
        // The conversion worked, store our result and move on to the next
        std::get<I>(aConvertedArgs) = result.unwrap();
        return PrepareArgsHelper<I + 1>(aArgs, aConvertedArgs);
      } else {
        // The conversion failed, return an error and don't continue
        return mozilla::Err(result.unwrapErr() +
                            nsPrintfCString(" (arg %ld)", I));
      }
    }
  }

  // Call the scaffolding function
  //
  // For async calls this should be called in the worker thread
  static CallResult CallScaffoldingFunc(ScaffoldingFunc aFunc,
                                        IntermediateArgs&& aArgs) {
    return CallScaffoldingFuncHelper(
        aFunc, std::move(aArgs), std::index_sequence_for<ArgConverters...>());
  }

  // Helper function for CallScaffoldingFunc that uses c++ magic to help with
  // iteration
  template <size_t... Is>
  static CallResult CallScaffoldingFuncHelper(ScaffoldingFunc aFunc,
                                              IntermediateArgs&& aArgs,
                                              std::index_sequence<Is...> seq) {
    CallResult result;

    auto makeCall = [&]() mutable {
      return aFunc(
          NthArgConverter<Is>::IntoRust(std::move(std::get<Is>(aArgs)))...,
          &result.mCallStatus);
    };
    if constexpr (std::is_void_v<typename ReturnConverter::RustType>) {
      makeCall();
    } else {
      result.mReturnValue = makeCall();
    }
    return result;
  }

  // Return the result of the scaffolding call back to JS
  //
  // This should be called on the main thread
  static void ReturnResult(
      JSContext* aContext, CallResult& aCallResult,
      dom::RootedDictionary<dom::UniFFIScaffoldingCallResult>& aReturnValue,
      const nsLiteralCString& aFuncName) {
    switch (aCallResult.mCallStatus.code) {
      case RUST_CALL_SUCCESS: {
        aReturnValue.mCode = dom::UniFFIScaffoldingCallCode::Success;
        if constexpr (!std::is_void_v<typename ReturnConverter::RustType>) {
          auto convertResult =
              ReturnConverter::FromRust(aCallResult.mReturnValue);
          if (convertResult.isOk()) {
            ReturnConverter::IntoJs(aContext, std::move(convertResult.unwrap()),
                                    aReturnValue.mData.Construct());
          } else {
            aReturnValue.mCode = dom::UniFFIScaffoldingCallCode::Internal_error;
            aReturnValue.mInternalErrorMessage.Construct(
                aFuncName + " converting result: "_ns +
                convertResult.unwrapErr());
          }
        }
        break;
      }

      case RUST_CALL_ERROR: {
        // Rust Err() value.  Populate data with the `RustBuffer` containing the
        // error
        aReturnValue.mCode = dom::UniFFIScaffoldingCallCode::Error;
        aReturnValue.mData.Construct().SetAsArrayBuffer().Init(
            OwnedRustBuffer(aCallResult.mCallStatus.error_buf)
                .IntoArrayBuffer(aContext));
        break;
      }

      default: {
        // This indicates a RustError, which shouldn't happen in practice since
        // FF sets panic=abort
        aReturnValue.mCode = dom::UniFFIScaffoldingCallCode::Internal_error;
        aReturnValue.mInternalErrorMessage.Construct(aFuncName +
                                                     " Unexpected Error"_ns);
        break;
      }
    }
  }
};

}  // namespace mozilla::uniffi

#endif  // mozilla_ScaffoldingCall_h
