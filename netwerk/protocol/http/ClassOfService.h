#ifndef __ClassOfService_h__
#define __ClassOfService_h__

#include "ipc/IPCMessageUtils.h"
// namespace IPC {}

namespace mozilla {
namespace net {

class ClassOfServiceStruct {
 public:
  ClassOfServiceStruct() : mClassFlags(0), mIncremental(false) {}
  ClassOfServiceStruct(unsigned long flags, bool incremental)
      : mClassFlags(flags), mIncremental(incremental) {}

  // class flags (priority)
  unsigned long Flags() const { return mClassFlags; }
  void SetFlags(unsigned long flags) { mClassFlags = flags; }

  // incremental flags
  bool Incremental() const { return mIncremental; }
  void SetIncremental(bool incremental) { mIncremental = incremental; }

 private:
  unsigned long mClassFlags;
  bool mIncremental;
  friend IPC::ParamTraits<mozilla::net::ClassOfServiceStruct>;
  friend bool operator==(const ClassOfServiceStruct& lhs,
                         const ClassOfServiceStruct& rhs);
  friend bool operator!=(const ClassOfServiceStruct& lhs,
                         const ClassOfServiceStruct& rhs);
};

inline bool operator==(const ClassOfServiceStruct& lhs,
                       const ClassOfServiceStruct& rhs) {
  return lhs.mClassFlags == rhs.mClassFlags &&
         lhs.mIncremental == rhs.mIncremental;
}

inline bool operator!=(const ClassOfServiceStruct& lhs,
                       const ClassOfServiceStruct& rhs) {
  return !(lhs == rhs);
}

}  // namespace net
}  // namespace mozilla

namespace IPC {
template <>
struct ParamTraits<mozilla::net::ClassOfServiceStruct> {
  typedef mozilla::net::ClassOfServiceStruct paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mClassFlags);
    WriteParam(aWriter, aParam.mIncremental);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &aResult->mClassFlags) ||
        !ReadParam(aReader, &aResult->mIncremental))
      return false;

    return true;
  }
};

}  // namespace IPC

#endif
