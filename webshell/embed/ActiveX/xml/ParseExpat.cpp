#include "stdafx.h"

#define XML_UNICODE
#include "xmlparse.h"

#ifdef XML_UNICODE
#define X2OLE W2COLE
#define X2T W2T
#else
#define X2OLE A2COLE
#define X2T A2T
#endif


struct ParserState
{
	CComQIPtr<IXMLDocument, &IID_IXMLDocument> spXMLDocument;
	CComQIPtr<IXMLElement, &IID_IXMLElement> spXMLRoot;
	CComQIPtr<IXMLElement, &IID_IXMLElement> spXMLParent;
};

static ParserState cParserState;


// XML data handlers
static void OnStartElement(void *userData, const XML_Char *name, const XML_Char **atts);
static void OnEndElement(void *userData, const XML_Char *name);
static void OnCharacterData(void *userData, const XML_Char *s, int len);
static void OnDefault(void *userData, const XML_Char *s, int len);


struct ParseData
{
	CComQIPtr<IXMLDocument, &IID_IXMLDocument> spDocument;
	CComQIPtr<IXMLElement, &IID_IXMLElement> spRoot;
};

HRESULT ParseExpat(const char *pBuffer, unsigned long cbBufSize, IXMLDocument *pDocument, IXMLElement **ppElement)
{
	if (pDocument == NULL)
	{
		return E_INVALIDARG;
	}

	XML_Parser parser = XML_ParserCreate(NULL);
	HRESULT hr = S_OK;

	cParserState.spXMLDocument = pDocument;
	pDocument->get_root(&cParserState.spXMLParent);

	// Initialise the XML parser
	XML_SetUserData(parser, &cParserState);

	// Initialise the data handlers
	XML_SetElementHandler(parser, OnStartElement, OnEndElement);
	XML_SetCharacterDataHandler(parser, OnCharacterData);
	XML_SetDefaultHandler(parser, OnDefault);

	// Parse the data
	if (!XML_Parse(parser, pBuffer, cbBufSize, 1))
	{
		/* TODO Create error code
		   fprintf(stderr,
			"%s at line %d\n",
		   XML_ErrorString(XML_GetErrorCode(parser)),
		   XML_GetCurrentLineNumber(parser));
		 */
		hr = E_FAIL;
	}

	// Cleanup
	XML_ParserFree(parser);

	cParserState.spXMLRoot->QueryInterface(IID_IXMLElement, (void **) ppElement);
	cParserState.spXMLDocument.Release();
	cParserState.spXMLParent.Release();

	return S_OK;
}


///////////////////////////////////////////////////////////////////////////////


void OnStartElement(void *userData, const XML_Char *name, const XML_Char **atts)
{
	ParserState *pState = (ParserState *) userData;
	if (pState)
	{
		USES_CONVERSION;

		CComQIPtr<IXMLElement, &IID_IXMLElement> spXMLElement;

		// Create a new element
		pState->spXMLDocument->createElement(
				CComVariant(XMLELEMTYPE_ELEMENT),
				CComVariant(X2OLE(name)),
				&spXMLElement);

		if (spXMLElement)
		{
			// Create each attribute
			for (int i = 0; atts[i] != NULL; i += 2)
			{
				const XML_Char *pszName = atts[i];
				const XML_Char *pszValue = atts[i+1];
				spXMLElement->setAttribute((BSTR) X2OLE(pszName), CComVariant(X2OLE(pszValue)));
			}

			if (pState->spXMLRoot == NULL)
			{
				pState->spXMLRoot = spXMLElement;
			}
			if (pState->spXMLParent)
			{
				// Add the element to the end of the list
				pState->spXMLParent->addChild(spXMLElement, -1, -1);
			}
			pState->spXMLParent = spXMLElement;
		}
	}
}


void OnEndElement(void *userData, const XML_Char *name)
{
	ParserState *pState = (ParserState *) userData;
	if (pState)
	{
		CComQIPtr<IXMLElement, &IID_IXMLElement> spNewParent;
		if (pState->spXMLParent)
		{
			pState->spXMLParent->get_parent(&spNewParent);
			pState->spXMLParent = spNewParent;
		}
	}
}


void OnDefault(void *userData, const XML_Char *s, int len)
{
	XML_Char *pString = new XML_Char[len + 1];
	memset(pString, 0, sizeof(XML_Char) * (len + 1));
	memcpy(pString, s, sizeof(XML_Char) * len);

	USES_CONVERSION;
	ATLTRACE(_T("OnDefault: \"%s\"\n"), X2T(pString));

	// TODO test if the buffer contains <?xml version="X"?>
	//      and store version in XML document

	// TODO test if the buffer contains DTD and store it
	//      in the XML document

	// TODO test if the buffer contains a comment, i.e. <!--.*-->
	//      and create a comment XML element

	delete []pString;
}


void OnCharacterData(void *userData, const XML_Char *s, int len)
{
	ParserState *pState = (ParserState *) userData;
	if (pState)
	{
		// TODO create TEXT element
	}
}


