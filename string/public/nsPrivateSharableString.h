#include "nsBufferHandle.h"

  /**
   *
   */
template <class CharT>
class nsPrivateSharableString
  {
    public:
      virtual const nsBufferHandle<CharT>*        GetBufferHandle() const;
      virtual const nsSharedBufferHandle<CharT>*  GetSharedBufferHandle() const;
  };

template <class CharT>
nsBufferHandle<CharT>*
nsPrivateSharableString<CharT>::GetBufferHandle() const
  {
    return GetSharedBufferHandle();
  }

template <class CharT>
nsBufferHandle<CharT>*
nsPrivateSharableString<CharT>::GetSharedBufferHandle() const
  {
    return 0;
  }
