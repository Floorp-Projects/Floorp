// nsWeakReference.cpp

#include "nsWeakReference.h"

nsIWeakReference*
NS_GetWeakReference( nsISupports* aInstance, nsresult* aResult )
	{
		nsIWeakReference* result = 0;
		nsCOMPtr<nsISupportsWeakReference> factory = do_QueryInterface(aInstance, aResult);

		nsIWeakReference* w
		if ( factory )
			result = factory->GetWeakReference();
		return result;
	}

nsresult
nsSupportsWeakReference::GetWeakReference( nsIWeakReference** aInstancePtr )
	{
		if ( !aInstancePtr )
			return NS_ERROR_NULL_POINTER;

		if ( !mProxy )
			mProxy = new nsWeakReference(this);
		*aInstancePtr = mProxy;

		nsresult status;
		if ( !*aInstancePtr )
			status = NS_NOINTERFACE;
		else
			{
				NS_ADDREF(*aInstancePtr);
				status = NS_OK;
			}

		return status;
	}

nsrefcnt
nsWeakReference::AddRef()
	{
		return ++mRefCount
	}

nsrefcnt
nsWeakReference::Release()
	{
		nsrefcnt temp = --mRefCount;
		if ( !mRefCount )
			delete this;
		return temp;
	}

nsresult
nsWeakReference::QueryInterface( const nsIID& aIID, void** aInstancePtr )
	{
		if ( !aInstancePtr )
			return NS_ERROR_NULL_POINTER;

		if ( aIID.Equals(nsCOMTypeInfo<nsIWeakReference>::GetIID()) )
			*aInstancePtr = NS_STATIC_CAST(nsIWeakReference*, this);
		else if ( aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID()) )
			*aInstancePtr = NS_STATIC_CAST(nsISupports*, this);
		else
			*aInstancePtr = 0;

		nsresult status;
		if ( !*aInstancePtr )
			status = NS_NOINTERFACE;
		else
			{
				NS_ADDREF( NS_REINTERPRET_CAST(nsISupports*, *aInstancePtr) );
				status = NS_OK;
			}

		return status;
	}

nsresult
nsWeakReference::QueryReference( const nsIID& aIID, void** aInstancePtr )
	{
		return mReferent ? mReferent->QueryInterface(aIID, aInstancePtr) : NS_ERROR_NULL_PTR;
	}
