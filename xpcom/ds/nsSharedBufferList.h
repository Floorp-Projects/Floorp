#include "nsBufferHandle.h"

  /**
   *
   */
class nsSharedBufferList
  {
    public:

      class Buffer
          : public nsSharedBufferHandle<PRUnichar>
        {
          friend class nsSharedBufferList;

          public:

          private:
            Buffer* mPrev;
            Buffer* mNext;
        };

      struct Position
        {
          nsSharedBufferList::Buffer*     mBuffer;
          nsSharedBufferList::PRUnichar*  mPosInBuffer;
        };



    public:
      nsSharedBufferList() : mFirstBuffer(0), mLastBuffer(0), mTotalLength(0) { }
      virtual ~nsSharedBufferList();

    private:
        // pass by value is not supported
      nsSharedBufferList( const nsSharedBufferList& );  // NOT TO BE IMPLEMENTED
      void operator=( const nsSharedBufferList& );      // NOT TO BE IMPLEMENTED

    public:
      void    LinkBuffer( Buffer*, Buffer*, Buffer* );
      Buffer* UnlinkBuffer( Buffer* );
      void    SplitBuffer( const Position& );

      Buffer* First() { return mFirstBuffer; }
      Buffer* Last()  { return mLastBuffer; }

    protected:
      void    DestroyBuffers();

    protected:
      Buffer*   mFirstBuffer;
      Buffer*   mLastBuffer;

      PRUint32  mTotalLength;
  };
