#include "stdafx.h"

#include "xmlparse.h"

struct ParserState
{
	CComQIPtr<IXMLDocument, &IID_IXMLDocument> spXMLDocument;
	CComQIPtr<IXMLElement, &IID_IXMLElement> spXMLParent;
};

ParserState cParserState;

static void OnStartElement(void *userData, const XML_Char *name, const XML_Char **atts);
static void OnEndElement(void *userData, const XML_Char *name);
static void OnCharacterData(void *userData, const XML_Char *s, int len);


HRESULT ParseExpat(const char *pBuffer, unsigned long cbBufSize, IXMLDocument *pDocument)
{
	if (pDocument == NULL)
	{
		return E_INVALIDARG;
	}

	XML_Parser parser = XML_ParserCreate(NULL);
	HRESULT hr = S_OK;

	cParserState.spXMLDocument = pDocument;

	// Initialise the XML parser
	XML_SetUserData(parser, &cParserState);
	XML_SetElementHandler(parser, OnStartElement, OnEndElement);
	XML_SetCharacterDataHandler(parser, OnCharacterData);

	// Parse the data
	if (!XML_Parse(parser, pBuffer, cbBufSize, 1))
	{
		/* TODO Print error message
		   fprintf(stderr,
			"%s at line %d\n",
		   XML_ErrorString(XML_GetErrorCode(parser)),
		   XML_GetCurrentLineNumber(parser));
		 */
		hr = E_FAIL;
	}

	// Cleanup
	XML_ParserFree(parser);
	cParserState.spXMLDocument.Release();
	cParserState.spXMLParent.Release();

	return S_OK;
}


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
				CComVariant(A2OLE(name)),
				&spXMLElement);

		if (spXMLElement)
		{
			// Create each attribute
			for (int i = 0; atts[i] != NULL; i += 2)
			{
				const XML_Char *pszName = atts[i];
				const XML_Char *pszValue = atts[i+1];
				spXMLElement->setAttribute(A2OLE(pszName), CComVariant(A2OLE(pszValue)));
			}

			// Add the element to the end of the list
			pState->spXMLParent->addChild(spXMLElement, -1, -1);
		}
	}
}


void OnEndElement(void *userData, const XML_Char *name)
{
}


void OnCharacterData(void *userData, const XML_Char *s, int len)
{
	ParserState *pState = (ParserState *) userData;
	if (pState)
	{
		// TODO create TEXT element
	}
}


