// Test05.cpp

#include "nsIDOMNode.h"
#include "nsCOMPtr.h"

#ifdef __MWERKS__
	#pragma exceptions off
#endif

NS_DEF_PTR(nsIDOMNode);

	/*
		Windows:
                        raw, nsCOMPtr           21 bytes

		Macintosh:
                        Raw, nsCOMPtr           64 bytes
	*/

class Test05_Raw
	{
		public:
                        Test05_Raw();
                 ~Test05_Raw();

			void /*nsresult*/ GetNode( nsIDOMNode** aNode );

		private:
			nsIDOMNode* mNode;
	};

Test05_Raw::Test05_Raw()
		: mNode(0)
	{
		// nothing else to do here
	}

Test05_Raw::~Test05_Raw()
	{
		NS_IF_RELEASE(mNode);
	}

void // nsresult
Test05_Raw::GetNode( nsIDOMNode** aNode )
		// m64, w21
	{
//		if ( !aNode )
//			return NS_ERROR_NULL_POINTER;

		*aNode = mNode;
		NS_IF_ADDREF(*aNode);

//		return NS_OK;
	}



class Test05_nsCOMPtr
	{
		public:
			void /*nsresult*/ GetNode( nsIDOMNode** aNode );

		private:
			nsCOMPtr<nsIDOMNode> mNode;
	};

void // nsresult
Test05_nsCOMPtr::GetNode( nsIDOMNode** aNode )
		// m64, w21
	{
//		if ( !aNode )
//			return NS_ERROR_NULL_POINTER;

		*aNode = mNode;
		NS_IF_ADDREF(*aNode);

//		return NS_OK;
	}
