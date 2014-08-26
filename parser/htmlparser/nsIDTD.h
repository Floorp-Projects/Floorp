/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIDTD_h___
#define nsIDTD_h___

/**
 * MODULE NOTES:
 * @update  gess 7/20/98
 *
 * This interface defines standard interface for DTD's. Note that this
 * isn't HTML specific. DTD's have several functions within the parser
 * system:
 *      1) To coordinate the consumption of an input stream via the
 *      parser
 *      2) To serve as proxy to represent the containment rules of the
 *      underlying document
 *      3) To offer autodetection services to the parser (mainly for doc
 *      conversion)
 * */

#include "nsISupports.h"
#include "nsString.h"
#include "nsITokenizer.h"

#define NS_IDTD_IID \
{ 0x3de05873, 0xefa7, 0x410d, \
  { 0xa4, 0x61, 0x80, 0x33, 0xaf, 0xd9, 0xe3, 0x26 } }

enum eAutoDetectResult {
    eUnknownDetect,
    eValidDetect,
    ePrimaryDetect,
    eInvalidDetect
};

enum nsDTDMode {
    eDTDMode_unknown = 0,
    eDTDMode_quirks,        //pre 4.0 versions
    eDTDMode_almost_standards,
    eDTDMode_full_standards,
    eDTDMode_autodetect,
    eDTDMode_fragment
};


class nsIParser;
class nsIURI;
class nsIContentSink;
class CParserContext;

class nsIDTD : public nsISupports
{
public:

    NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDTD_IID)

    NS_IMETHOD WillBuildModel(const CParserContext& aParserContext,
                              nsITokenizer* aTokenizer,
                              nsIContentSink* aSink) = 0;

    /**
     * Called by the parser after the parsing process has concluded
     * @update  gess5/18/98
     * @param   anErrorCode - contains error code resulting from parse process
     * @return
     */
    NS_IMETHOD DidBuildModel(nsresult anErrorCode) = 0;

    /**
     * Called (possibly repeatedly) by the parser to parse tokens and construct
     * the document model via the sink provided to WillBuildModel.
     *
     * @param   aTokenizer - tokenizer providing the token stream to be parsed
     * @param   aCountLines - informs the DTD whether to count newlines
     *                        (not wanted, e.g., when handling document.write)
     * @param   aCharsetPtr - address of an nsCString containing the charset
     *                        that the DTD should use (pointer in case the DTD
     *                        opts to ignore this parameter)
     */
    NS_IMETHOD BuildModel(nsITokenizer* aTokenizer, nsIContentSink* aSink) = 0;

    /**
     * This method is called to determine whether or not a tag of one
     * type can contain a tag of another type.
     *
     * @update  gess 3/25/98
     * @param   aParent -- int tag of parent container
     * @param   aChild -- int tag of child container
     * @return true if parent can contain child
     */
    NS_IMETHOD_(bool) CanContain(int32_t aParent,int32_t aChild) const = 0;

    /**
     * This method gets called to determine whether a given
     * tag is itself a container
     *
     * @update  gess 3/25/98
     * @param   aTag -- tag to test for containership
     * @return  true if given tag can contain other tags
     */
    NS_IMETHOD_(bool) IsContainer(int32_t aTag) const = 0;

    /**
     * Use this id you want to stop the building content model
     * --------------[ Sets DTD to STOP mode ]----------------
     * It's recommended to use this method in accordance with
     * the parser's terminate() method.
     *
     * @update  harishd 07/22/99
     * @param
     * @return
     */
    NS_IMETHOD_(void) Terminate() = 0;

    NS_IMETHOD_(int32_t) GetType() = 0;

    /**
     * Call this method after calling WillBuildModel to determine what mode the
     * DTD actually is using, as it may differ from aParserContext.mDTDMode.
     */
    NS_IMETHOD_(nsDTDMode) GetMode() const = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDTD, NS_IDTD_IID)

#define NS_DECL_NSIDTD \
    NS_IMETHOD WillBuildModel(  const CParserContext& aParserContext, nsITokenizer* aTokenizer, nsIContentSink* aSink);\
    NS_IMETHOD DidBuildModel(nsresult anErrorCode);\
    NS_IMETHOD BuildModel(nsITokenizer* aTokenizer, nsIContentSink* aSink);\
    NS_IMETHOD_(bool) CanContain(int32_t aParent,int32_t aChild) const;\
    NS_IMETHOD_(bool) IsContainer(int32_t aTag) const;\
    NS_IMETHOD_(void)  Terminate();\
    NS_IMETHOD_(int32_t) GetType();\
    NS_IMETHOD_(nsDTDMode) GetMode() const;
#endif /* nsIDTD_h___ */
