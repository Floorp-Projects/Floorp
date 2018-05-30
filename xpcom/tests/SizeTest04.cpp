// Test04.cpp

#include "nsINode.h"
#include "nsCOMPtr.h"

NS_DEF_PTR(nsINode);

	/*
		Windows:
			nsCOMPtr							 13
			raw										 36

		Macintosh:
			nsCOMPtr							 36 bytes		(1.0000)
			raw										120					(3.3333)	i.e., 333.33% bigger than nsCOMPtr
	*/

class Test04_Raw
	{
		public:
			Test04_Raw();
		 ~Test04_Raw();

			void /*nsresult*/ SetNode( nsINode* newNode );

		private:
			nsINode* mNode;
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
Test04_Raw::SetNode( nsINode* newNode )
		// m120, w36
	{
		NS_IF_ADDREF(newNode);
		NS_IF_RELEASE(mNode);
		mNode = newNode;

//		return NS_OK;
	}



class Test04_nsCOMPtr
	{
		public:
			void /*nsresult*/ SetNode( nsINode* newNode );

		private:
			nsCOMPtr<nsINode> mNode;
	};

void // nsresult
Test04_nsCOMPtr::SetNode( nsINode* newNode )
		// m36, w13/13
	{
		mNode = newNode;
	}
