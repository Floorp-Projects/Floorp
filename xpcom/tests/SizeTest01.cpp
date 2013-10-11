// Test01.cpp

#include "nsIDOMNode.h"
#include "nsCOMPtr.h"
#include "nsString.h"

NS_DEF_PTR(nsIDOMNode);

	/*
		This test file compares the generated code size of similar functions between raw
                COM interface pointers (|AddRef|ing and |Release|ing by hand) and |nsCOMPtr|s.

		Function size results were determined by examining dissassembly of the generated code.
		mXXX is the size of the generated code on the Macintosh.  wXXX is the size on Windows.
		For these tests, all reasonable optimizations were enabled and exceptions were
		disabled (just as we build for release).

		The tests in this file explore only the simplest functionality: assigning a pointer
                to be reference counted into a [raw, nsCOMPtr] object; ensuring that it is
		|AddRef|ed and |Release|d appropriately; calling through the pointer to a function
		supplied by the underlying COM interface.

		Windows:
			raw_optimized														 31 bytes
			raw, nsCOMPtr*													 34
			nsCOMPtr_optimized*											 38
			nsCOMPtr_optimized											 42
			nsCOMPtr																 46

		Macintosh:
			raw_optimized, nsCOMPtr_optimized				112 bytes 	(1.0000)
			nsCOMPtr																120					(1.0714)	i.e., 7.14% bigger than raw_optimized et al
			raw																			140 				(1.2500)

		The overall difference in size between Windows and Macintosh is caused by the
		the PowerPC RISC architecture where every instruction is 4 bytes.

                On Macintosh, nsCOMPtr generates out-of-line destructors which are
		not referenced, and which can be stripped by the linker.
	*/

void
Test01_raw( nsIDOMNode* aDOMNode, nsString* aResult )
		// m140, w34
	{
			/*
				This test is designed to be more like a typical large function where,
				because you are working with several resources, you don't just return when
				one of them is |nullptr|.  Similarly: |Test01_nsCOMPtr00|, and |Test01_nsIPtr00|.
			*/

		nsIDOMNode* node = aDOMNode;
		NS_IF_ADDREF(node);

		if ( node )
			node->GetNodeName(*aResult);

		NS_IF_RELEASE(node);
	}

void
Test01_raw_optimized( nsIDOMNode* aDOMNode, nsString* aResult )
		// m112, w31
	{
			/*
				This test simulates smaller functions where you _do_ just return
				|nullptr| at the first sign of trouble.  Similarly: |Test01_nsCOMPtr01|,
				and |Test01_nsIPtr01|.
			*/

			/*
				This test produces smaller code that |Test01_raw| because it avoids
				the three tests: |NS_IF_...|, and |if ( node )|.
			*/

// -- the following code is assumed, but is commented out so we compare only
//		 the relevent generated code

//		if ( !aDOMNode )
//			return;

		nsIDOMNode* node = aDOMNode;
		NS_ADDREF(node);
		node->GetNodeName(*aResult);
		NS_RELEASE(node);
	}

void
Test01_nsCOMPtr( nsIDOMNode* aDOMNode, nsString* aResult )
		// m120, w46/34
	{
		nsCOMPtr<nsIDOMNode> node = aDOMNode;

		if ( node )
			node->GetNodeName(*aResult);
	}

void
Test01_nsCOMPtr_optimized( nsIDOMNode* aDOMNode, nsString* aResult )
		// m112, w42/38
	{
//		if ( !aDOMNode )
//			return;

		nsCOMPtr<nsIDOMNode> node = aDOMNode;
		node->GetNodeName(*aResult);
	}
