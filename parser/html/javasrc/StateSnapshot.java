/*
 * Copyright (c) 2009-2010 Mozilla Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a 
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 */

package nu.validator.htmlparser.impl;

import nu.validator.htmlparser.annotation.Auto;


public class StateSnapshot<T> implements TreeBuilderState<T> {

    private final @Auto StackNode<T>[] stack;

    private final @Auto StackNode<T>[] listOfActiveFormattingElements;

    private final @Auto int[] templateModeStack;

    private final T formPointer;

    private final T headPointer;

    private final T deepTreeSurrogateParent;

    private final int mode;

    private final int originalMode;
    
    private final boolean framesetOk;

    private final boolean needToDropLF;

    private final boolean quirks;

    /**
     * @param stack
     * @param listOfActiveFormattingElements
     * @param templateModeStack
     * @param formPointer
     * @param headPointer
     * @param deepTreeSurrogateParent
     * @param mode
     * @param originalMode
     * @param framesetOk
     * @param needToDropLF
     * @param quirks
     */
    StateSnapshot(StackNode<T>[] stack,
            StackNode<T>[] listOfActiveFormattingElements, int[] templateModeStack, T formPointer,
            T headPointer, T deepTreeSurrogateParent, int mode, int originalMode,
            boolean framesetOk, boolean needToDropLF, boolean quirks) {
        this.stack = stack;
        this.listOfActiveFormattingElements = listOfActiveFormattingElements;
        this.templateModeStack = templateModeStack;
        this.formPointer = formPointer;
        this.headPointer = headPointer;
        this.deepTreeSurrogateParent = deepTreeSurrogateParent;
        this.mode = mode;
        this.originalMode = originalMode;
        this.framesetOk = framesetOk;
        this.needToDropLF = needToDropLF;
        this.quirks = quirks;
    }
    
    /**
     * @see nu.validator.htmlparser.impl.TreeBuilderState#getStack()
     */
    public StackNode<T>[] getStack() {
        return stack;
    }

    /**
     * @see nu.validator.htmlparser.impl.TreeBuilderState#getTemplateModeStack()
     */
    public int[] getTemplateModeStack() {
        return templateModeStack;
    }

    /**
     * @see nu.validator.htmlparser.impl.TreeBuilderState#getListOfActiveFormattingElements()
     */
    public StackNode<T>[] getListOfActiveFormattingElements() {
        return listOfActiveFormattingElements;
    }

    /**
     * @see nu.validator.htmlparser.impl.TreeBuilderState#getFormPointer()
     */
    public T getFormPointer() {
        return formPointer;
    }

    /**
     * Returns the headPointer.
     * 
     * @return the headPointer
     */
    public T getHeadPointer() {
        return headPointer;
    }

    /**
     * Returns the deepTreeSurrogateParent.
     * 
     * @return the deepTreeSurrogateParent
     */
    public T getDeepTreeSurrogateParent() {
        return deepTreeSurrogateParent;
    }
    
    /**
     * Returns the mode.
     * 
     * @return the mode
     */
    public int getMode() {
        return mode;
    }

    /**
     * Returns the originalMode.
     * 
     * @return the originalMode
     */
    public int getOriginalMode() {
        return originalMode;
    }

    /**
     * Returns the framesetOk.
     * 
     * @return the framesetOk
     */
    public boolean isFramesetOk() {
        return framesetOk;
    }

    /**
     * Returns the needToDropLF.
     * 
     * @return the needToDropLF
     */
    public boolean isNeedToDropLF() {
        return needToDropLF;
    }

    /**
     * Returns the quirks.
     * 
     * @return the quirks
     */
    public boolean isQuirks() {
        return quirks;
    }
    
    /**
     * @see nu.validator.htmlparser.impl.TreeBuilderState#getListOfActiveFormattingElementsLength()
     */
    public int getListOfActiveFormattingElementsLength() {
        return listOfActiveFormattingElements.length;
    }

    /**
     * @see nu.validator.htmlparser.impl.TreeBuilderState#getStackLength()
     */
    public int getStackLength() {
        return stack.length;
    }

    /**
     * @see nu.validator.htmlparser.impl.TreeBuilderState#getTemplateModeStackLength()
     */
    public int getTemplateModeStackLength() {
        return templateModeStack.length;
    }

    @SuppressWarnings("unused") private void destructor() {
        for (int i = 0; i < stack.length; i++) {
            stack[i].release();
        }
        for (int i = 0; i < listOfActiveFormattingElements.length; i++) {
            if (listOfActiveFormattingElements[i] != null) {
                listOfActiveFormattingElements[i].release();                
            }
        }
    }
}
