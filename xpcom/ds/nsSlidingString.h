#include "nsAReadableString.h"
#include "nsSharedBufferList.h"


  /**
   * Maintains the sequence from the prev-most referenced buffer to the last buffer.
   * As prev-most buffers become un-referenced, they are unlinked from the list
   * and destroyed.
   *
   * One or more |nsSlidingSubstring|s may reference into the list.  Each |nsSlidingSubstring|
   * holds a reference into the prev-most buffer intersecting the 
   * substring it describes.  The destructor of a |nsSlidingSubstring| releases this
   * reference, allowing the buffer list to destroy the contiguous prefix of
   * unreferenced buffers.
   *
   * A single instance of |nsSlidingString| may reference this list.
   * Through that interface, new data can be appended onto the next-most end
   * of the list.  |nsSlidingString| also the client to advance its starting point.
   * 
   */
class nsSlidingSharedBufferList
    : public nsSharedBufferList
  {
    public:
      nsSlidingSharedBufferList() : mRefCount(0) { }

      void  AcquireReference()    { ++mRefCount; }
      void  ReleaseReference()    { if ( !--mRefCount ) delete this; }

      void  DiscardUnreferencedPrefix( Buffer* );

    private:
      PRUint32 mRefCount;
  };





  /**
   * a substring over a buffer list, this 
   */
class nsSlidingSubstring
     : public nsPromiseReadable<PRUnichar>
  {
    friend class nsSlidingSharedBufferList;

    public:
      nsSlidingSubstring( nsSharedBufferList* aBufferList, const nsSharedBufferList::Position& aStart, const nsSharedBufferList::Position& aEnd );
      nsSlidingSubstring( nsSharedBufferList* aBufferList );
     ~nsSlidingSubstring();

      // need to take care of
      //  copy-constructor
      //  copy-assignment

      virtual PRUint32 Length() const { return mLength; }

    protected:
      virtual const PRUnichar* GetReadableFragment( nsReadableFragment<PRUnichar>&, nsFragmentRequest, PRUint32 );
      
    protected:
      nsSharedBufferList::Position  mStart, mEnd;
      nsSharedBufferList&           mBufferList;
      PRUint32                      mLength;
  };




  /**
   * An |nsSlidingSharedBufferList| may be modified by zero or one instances of this class.
   *  
   */
class nsSlidingString
    : public nsSlidingSubstring
  {
    public:
      nsSlidingString();

        // you are giving ownership to the string, it takes and keeps your buffer, deleting it when done
      void AppendBuffer( PRUnichar* aStorageStart, PRUnichar* aDataEnd, PRUnichar* aStorageEnd );
//    void Append( ... ); do you want some |Append|s that copy the supplied data?

      void DiscardPrefix( const nsReadingIterator<PRUnichar>& );
        // any other way you want to do this?
  };


typedef nsSlidingString     nsParserString;
typedef nsSlidingSubstring  nsParserSubstring;