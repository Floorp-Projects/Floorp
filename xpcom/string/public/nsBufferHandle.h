

  /**
   *
   */
template <class CharT>
class nsBufferHandle
  {
    public:

      ptrdiff_t
      DataLength() const
        {
          return mDataEnd - mDataStart;
        }

      const CharT*
      DataStart() const
        {
          return mDataStart;
        }

      const CharT*
      DataEnd() const
        {
          return mDataEnd;
        }

    protected:
      const CharT*  mDataStart;
      const CharT*  mDataEnd;
  };



  /**
   *
   */
template <class CharT>
class nsSharedBufferHandle
    : public nsStringFragmentHandle<CharT>
  {
    public:
      ~nsSharedBufferHandle()
        {
          NS_ASSERTION(!IsReferenced(), "!IsReferenced()");

          if ( mFlags & kDeleteStorageFlag )
            nsMemory::Free(mDataStart);
              // OK, this is part of a hack.  Here's what's going on
              
              // In general, a handle points to a hunk of data where the data
              // must start at the beginning of the allocated storage ... therefore,
              // that's what we deallocate when we're going away.  We don't want
              // any |virtual| functions, because we don't want to add a vptr to this
              // structure.  So, later, when we have a more advanced version of this
              // class that allows the real data to be an arbitrary sub-range of the
              // allocated space, it's destructor will need to fix the data pointer
              // to point to the storage to be de-allocated before it gets to here.
        }

      void
      AcquireReference() const
        {
          set_refcount( get_refcount()+1 );
        }

      void
      ReleaseReference() const
        {
          if ( !set_refcount( get_refcount()-1 ) )
            delete this;
        }

      PRBool
      IsReferenced() const
        {
          return get_refcount() != 0;
        }

    protected:
      PRUint32      mFlags;

      enum
        {
          kIsShared
          kIsSingleAllocationWithBuffer

          kDeleteStorageFlag  = 1<<31,
          kOffsetDataFlag     = 1<<30,

          // room for other flags

          kFlagsMask          = kDeleteStorageFlag | kOffsetDataFlag,
          kRefCountMask       = ~kFlagsMask
        };

      PRUint32
      get_refcount() const
        {
          return mFlags & kRefCountMask;
        }

      PRUint32
      set_refcount( PRUint32 aNewRefCount )
        {
          NS_ASSERTION(aNewRefCount <= kRefCountMask, "aNewRefCount <= kRefCountMask");

          mFlags = (mFlags & kFlagsMask) | aNewRefCount;
          return aNewRefCount;
        }
  };


#if 0
template <class CharT>
class ns...
    : public nsSharedBufferHandle<CharT>
  {
    public:
      ~ns...()
        {
          mDataStart = mStorageStart;
            // fix up |mDataStart| to point to the thing to be deallocated
            // see the note above

            // The problem is, since the destructor isn't virtual, that such an
            // object must be destroyed in the static scope that knows its correct type
        }

    protected:
      const CharT*  mStorageStart;
      const CharT*  mStorageEnd;
  };
#endif
