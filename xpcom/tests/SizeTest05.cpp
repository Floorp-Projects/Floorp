// Test05.cpp

#include "nsIDOMNode.h"
#include "nsCOMPtr.h"
#include "nsIPtr.h"

#ifdef __MWERKS__
	#pragma exceptions off
#endif

NS_DEF_PTR(nsIDOMNode);

	/*
		Windows:
			raw, nsCOMPtr, nsIPtr		21 bytes

		Macintosh:
			Raw, nsCOMPtr, nsIPtr		64 bytes
	*/

class Test04_Raw
	{
		public:
			Test04_Raw();
		 ~Test04_Raw();

			void /*nsresult*/ GetNode( nsIDOMNode** aNode );

		private:
			nsIDOMNode* mNode;
	};

Test04_Raw::Test04_Raw()
		: mNode(0)
	{
		// nothing else to do here
	}

Test04_Raw::~Test04_Raw()
	{
		NS_IF_RELEASE(mNode);
	}

void // nsresult
Test04_Raw::GetNode( nsIDOMNode** aNode )
		// m64, w21
	{
//		if ( !aNode )
//			return NS_ERROR_NULL_POINTER;

		*aNode = mNode;
		NS_IF_ADDREF(*aNode);

//		return NS_OK;
	}



class Test04_nsCOMPtr
	{
		public:
			void /*nsresult*/ GetNode( nsIDOMNode** aNode );

		private:
			nsCOMPtr<nsIDOMNode> mNode;
	};

void // nsresult
Test04_nsCOMPtr::GetNode( nsIDOMNode** aNode )
		// m64, w21
	{
//		if ( !aNode )
//			return NS_ERROR_NULL_POINTER;

		*aNode = mNode;
		NS_IF_ADDREF(*aNode);

//		return NS_OK;
	}



class Test04_nsIPtr
	{
		public:
			void /*nsresult*/ GetNode( nsIDOMNode** aNode );

		private:
			nsIDOMNodePtr mNode;
	};

void // nsresult
Test04_nsIPtr::GetNode( nsIDOMNode** aNode )
		// m64, w21
	{
//		if ( !aNode )
//			return NS_ERROR_NULL_POINTER;

		*aNode = mNode;
		NS_IF_ADDREF(*aNode);

//		return NS_OK;
	}
