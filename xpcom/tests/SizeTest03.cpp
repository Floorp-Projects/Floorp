// Test03.cpp

#include "nsIDOMNode.h"
#include "nsCOMPtr.h"
#include "nsString.h"

NS_DEF_PTR(nsIDOMNode);

	/*
		Windows:
			nsCOMPtr_optimized*											 45
			raw_optimized														 48
			nsCOMPtr_optimized											 50
			nsCOMPtr																 54
			nsCOMPtr*																 59
			raw																			 62

		Macintosh:
			nsCOMPtr_optimized		112					(1.0000)
			raw_optimized					124 bytes		(1.1071)	i.e., 10.71% bigger than nsCOMPtr_optimized
			nsCOMPtr							144					(1.2857)
	*/

void // nsresult
Test03_raw( nsIDOMNode* aDOMNode, nsString* aResult )
		// m140, w62
	{
// -- the following code is assumed, but is commented out so we compare only
//		 the relevent generated code

//		if ( !aDOMNode || !aResult )
//			return NS_ERROR_NULL_POINTER;

		nsIDOMNode* parent = 0;
		nsresult status = aDOMNode->GetParentNode(&parent);

		if ( NS_SUCCEEDED(status) )
			{
				parent->GetNodeName(*aResult);
			}

		NS_IF_RELEASE(parent);

//		return status;
	}


void // nsresult
Test03_raw_optimized( nsIDOMNode* aDOMNode, nsString* aResult )
		// m124, w48
	{
//		if ( !aDOMNode || !aResult )
//			return NS_ERROR_NULL_POINTER;

		nsIDOMNode* parent;
		nsresult status = aDOMNode->GetParentNode(&parent);

		if ( NS_SUCCEEDED(status) )
			{
				parent->GetNodeName(*aResult);
				NS_RELEASE(parent);
			}

//		return status;
	}


void // nsresult
Test03_nsCOMPtr( nsIDOMNode* aDOMNode, nsString* aResult )
		// m144, w54/59
	{
//		if ( !aDOMNode || !aResult )
//			return NS_ERROR_NULL_POINTER;

		nsCOMPtr<nsIDOMNode> parent;
		nsresult status = aDOMNode->GetParentNode( getter_AddRefs(parent) );
		if ( parent )
			parent->GetNodeName(*aResult);

//		return status;
	}

void // nsresult
Test03_nsCOMPtr_optimized( nsIDOMNode* aDOMNode, nsString* aResult )
		// m112, w50/45
	{
//		if ( !aDOMNode || !aResult )
//			return NS_ERROR_NULL_POINTER;

		nsIDOMNode* temp;
		nsresult status = aDOMNode->GetParentNode(&temp);
		nsCOMPtr<nsIDOMNode> parent( dont_AddRef(temp) );
		if ( parent )
			parent->GetNodeName(*aResult);

//		return status;
	}
