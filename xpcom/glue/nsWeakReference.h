#ifndef nsWeakReference_h__
#define nsWeakReference_h__

// nsWeakReference.h

#include "nsIWeakReference.h"

class nsSupportsWeakReference : public nsISupportsWeakReference
	{
		public:
			nsSupportsWeakReference()
					: mProxy(0)
				{
					// nothing else to do here
				}

			inline ~nsSupportsWeakReference();

			virtual nsresult GetWeakReference( nsIWeakReference** );

		private:
			friend class nsWeakReference;

			void
			NoticeProxyDestruction()
					// ...called (only) by an |nsWeakReference| from _its_ dtor.
				{
					mProxy = 0;
				}

			nsWeakReference* mProxy;
	};

class nsWeakReference : public nsIWeakReference
	{
		public:
		// nsISupports...
			virtual nsrefcnt AddRef();
			virtual nsrefcnt Release();
			virtual nsresult QueryInterface( const nsIID&, void** );

		// nsIWeakReference...
			virtual nsresult QueryReference( const nsIID&, void** );


		private:
			friend class nsSupportsWeakReference;

			nsWeakReference( nsSupportsWeakReference* referent )
					: mRefCount(0),
						mReferent(referent)
					// ...I can only be constructed by an |nsSupportsWeakReference|
				{
					// nothing else to do here
				}

		 ~nsWeakReference()
		 			// ...I will only be destroyed by calling |delete| myself.
				{
					if ( mReferent )
						mReferent->NoticeProxyDestruction();
				}

			void
			NoticeReferentDestruction()
					// ...called (only) by an |nsSupportsWeakReference| from _its_ dtor.
				{
					mReferent = 0;
				}

			nsrefcnt									mRefCount;
			nsSupportsWeakReference*	mReferent;
	};

inline
nsSupportsWeakReference::~nsSupportsWeakReference()
	{
		if ( mProxy )
			mProxy->NoticeReferentDestruction();
	}

#endif
