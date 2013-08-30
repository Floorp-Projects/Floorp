/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAHtml5TreeBuilderState_h
#define nsAHtml5TreeBuilderState_h

/**
 * Interface for exposing the internal state of the HTML5 tree builder.
 * For more documentation, please see
 * http://hg.mozilla.org/projects/htmlparser/file/tip/src/nu/validator/htmlparser/impl/StateSnapshot.java
 */
class nsAHtml5TreeBuilderState {
  public:
  
    virtual jArray<nsHtml5StackNode*,int32_t> getStack() = 0;
    
    virtual jArray<nsHtml5StackNode*,int32_t> getListOfActiveFormattingElements() = 0;

    virtual jArray<int32_t,int32_t> getTemplateModeStack() = 0;

    virtual int32_t getStackLength() = 0;

    virtual int32_t getListOfActiveFormattingElementsLength() = 0;

    virtual int32_t getTemplateModeStackLength() = 0;

    virtual nsIContent** getFormPointer() = 0;
    
    virtual nsIContent** getHeadPointer() = 0;

    virtual nsIContent** getDeepTreeSurrogateParent() = 0;

    virtual int32_t getMode() = 0;

    virtual int32_t getOriginalMode() = 0;

    virtual bool isFramesetOk() = 0;

    virtual bool isNeedToDropLF() = 0;

    virtual bool isQuirks() = 0;
    
    virtual ~nsAHtml5TreeBuilderState() {
    }
};

#endif /* nsAHtml5TreeBuilderState_h */
