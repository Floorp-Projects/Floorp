// Test05.cpp

#include "nsINode.h"
#include "nsCOMPtr.h"

NS_DEF_PTR(nsINode);

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

			void /*nsresult*/ GetNode( nsINode** aNode );

		private:
			nsINode* mNode;
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
Test05_Raw::GetNode( nsINode** aNode )
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
			void /*nsresult*/ GetNode( nsINode** aNode );

		private:
			nsCOMPtr<nsINode> mNode;
	};

void // nsresult
Test05_nsCOMPtr::GetNode( nsINode** aNode )
		// m64, w21
	{
//		if ( !aNode )
//			return NS_ERROR_NULL_POINTER;

		*aNode = mNode;
		NS_IF_ADDREF(*aNode);

//		return NS_OK;
	}
