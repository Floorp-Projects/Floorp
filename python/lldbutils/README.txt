lldb debugging functionality for Gecko
======================================

This directory contains a module, lldbutils, which is imported by the
in-tree .lldbinit file.  The lldbutil modules define some lldb commands
that are handy for debugging Gecko.

If you want to add a new command or Python-implemented type summary, either add
it to one of the existing broad area Python files (such as lldbutils/layout.py
for layout-related commands) or create a new file if none of the existing files
is appropriate.  If you add a new file, make sure you add it to __all__ in
lldbutils/__init__.py.


Supported commands
------------------

Most commands below that can take a pointer to an object also support being
called with a smart pointer like nsRefPtr or nsCOMPtr.


* frametree EXPR, ft EXPR
  frametreelimited EXPR, ftl EXPR

  Shows information about a frame tree.  EXPR is an expression that
  is evaluated, and must be an nsIFrame*.  frametree displays the
  entire frame tree that contains the given frame.  frametreelimited
  displays a subtree of the frame tree rooted at the given frame.

  (lldb) p this
  (nsBlockFrame *) $4 = 0x000000011687fcb8
  (lldb) ftl this
  Block(div)(-1)@0x11687fcb8 {0,0,7380,690} [state=0002100000d04601] [content=0x11688c0c0] [sc=0x11687f990:-moz-scrolled-content]<
    line 0x116899130: count=1 state=inline,clean,prevmarginclean,not impacted,not wrapped,before:nobr,after:nobr[0x100] {60,0,0,690} vis-overflow=60,510,0,0 scr-overflow=60,510,0,0 <
      Text(0)""@0x1168990c0 {60,510,0,0} [state=0001000020404000] [content=0x11687ca10] [sc=0x11687fd88:-moz-non-element,parent=0x11687eb00] [run=0x115115e80][0,0,T]
    >
  >
  (lldb) ft this
  Viewport(-1)@0x116017430 [view=0x115efe190] {0,0,60,60} [state=000b063000002623] [sc=0x1160170f8:-moz-viewport]<
    HTMLScroll(html)(-1)@0x1160180d0 {0,0,0,0} [state=000b020000000403] [content=0x115e4d640] [sc=0x116017768:-moz-viewport-scroll]<
      ...
      Canvas(html)(-1)@0x116017e08 {0,0,60,60} vis-overflow=0,0,8340,2196 scr-overflow=0,0,8220,2196 [state=000b002000000601] [content=0x115e4d640] [sc=0x11687e0f8:-moz-scrolled-canvas]<
        Block(html)(-1)@0x11687e578 {0,0,60,2196} vis-overflow=0,0,8340,2196 scr-overflow=0,0,8220,2196 [state=000b100000d00601] [content=0x115e4d640] [sc=0x11687e4b8,parent=0x0]<
          line 0x11687ec48: count=1 state=block,clean,prevmarginclean,not impacted,not wrapped,before:nobr,after:nobr[0x48] bm=480 {480,480,0,1236} vis-overflow=360,426,7980,1410 scr-overflow=480,480,7740,1236 <
            Block(body)(1)@0x11687ebb0 {480,480,0,1236} vis-overflow=-120,-54,7980,1410 scr-overflow=0,0,7740,1236 [state=000b120000100601] [content=0x115ed8980] [sc=0x11687e990]<
              line 0x116899170: count=1 state=inline,clean,prevmarginclean,not impacted,not wrapped,before:nobr,after:nobr[0x0] {0,0,7740,1236} vis-overflow=-120,-54,7980,1410 scr-overflow=0,0,7740,1236 <
                nsTextControlFrame@0x11687f068 {0,66,7740,1170} vis-overflow=-120,-120,7980,1410 scr-overflow=0,0,7740,1170 [state=0002000000004621] [content=0x115ca2c50] [sc=0x11687ea40]<
                  HTMLScroll(div)(-1)@0x11687f6b0 {180,240,7380,690} [state=0002000000084409] [content=0x11688c0c0] [sc=0x11687eb00]<
                    Block(div)(-1)@0x11687fcb8 {0,0,7380,690} [state=0002100000d04601] [content=0x11688c0c0] [sc=0x11687f990:-moz-scrolled-content]<
                      line 0x116899130: count=1 state=inline,clean,prevmarginclean,not impacted,not wrapped,before:nobr,after:nobr[0x100] {60,0,0,690} vis-overflow=60,510,0,0 scr-overflow=60,510,0,0 <
                        Text(0)""@0x1168990c0 {60,510,0,0} [state=0001000020404000] [content=0x11687ca10] [sc=0x11687fd88:-moz-non-element,parent=0x11687eb00] [run=0x115115e80][0,0,T]
 ...


* js

  Dumps the current JS stack.

  (lldb) js
  0 anonymous(aForce = false) ["chrome://browser/content/browser.js":13414]
      this = [object Object]
  1 updateAppearance() ["chrome://browser/content/browser.js":13326]
      this = [object Object]
  2 handleEvent(aEvent = [object Event]) ["chrome://browser/content/tabbrowser.xml":3811]
      this = [object XULElement]


* prefcnt EXPR

  Shows the refcount of a given object.  EXPR is an expression that is
  evaluated, and can be either a pointer to or an actual refcounted
  object.  The object can be a standard nsISupports-like refcounted
  object, a cycle-collected object or a mozilla::RefCounted<T> object.

  (lldb) p this
  (nsHTMLDocument *) $1 = 0x0000000116e9d800
  (lldb) prefcnt this
  20
  (lldb) p mDocumentURI
  (nsCOMPtr<nsIURI>) $3 = {
    mRawPtr = 0x0000000117163e50
  }
  (lldb) prefcnt mDocumentURI
  11


* pstate EXPR

  Shows the frame state bits (using their symbolic names) of a given frame.
  EXPR is an expression that is evaluated, and must be an nsIFrame*.

  (lldb) p this
  (nsTextFrame *) $1 = 0x000000011f470b10
  (lldb) p/x mState
  (nsFrameState) $2 = 0x0000004080604000
  (lldb) pstate this
  TEXT_HAS_NONCOLLAPSED_CHARACTERS | TEXT_END_OF_LINE | TEXT_START_OF_LINE | NS_FRAME_PAINTED_THEBES | NS_FRAME_INDEPENDENT_SELECTION


* ptag EXPR

  Shows the DOM tag name of a node.  EXPR is an expression that is
  evaluated, and can be either an nsINode pointer or a concrete DOM
  object.

  (lldb) p this
  (nsHTMLDocument *) $0 = 0x0000000116e9d800
  (lldb) ptag this
  (PermanentAtomImpl *) $1 = 0x0000000110133ac0 u"#document"
  (lldb) p this->GetRootElement()
  (mozilla::dom::HTMLSharedElement *) $2 = 0x0000000118429780
  (lldb) ptag $2
  (PermanentAtomImpl *) $3 = 0x0000000110123b80 u"html"


Supported type summaries and synthetic children
-----------------------------------------------

In lldb terminology, type summaries are rules for how to display a value
when using the "expression" command (or its familiar-to-gdb-users "p" alias),
and synthetic children are fake member variables or array elements also
added by custom rules.

For objects that do have synthetic children defined for them, like nsTArray,
the "expr -R -- EXPR" command can be used to show its actual member variables.


* nsAString, nsACString,
  nsFixedString, nsFixedCString,
  nsAutoString, nsAutoCString

  Strings have a type summary that shows the actual string.

  (lldb) frame info
  frame #0: 0x000000010400cfea XUL`nsCSSParser::ParseProperty(this=0x00007fff5fbf5248, aPropID=eCSSProperty_margin_top, aPropValue=0x00007fff5fbf53f8, aSheetURI=0x0000000115ae8c00, aBaseURI=0x0000000115ae8c00, aSheetPrincipal=0x000000010ff9e040, aDeclaration=0x00000001826fd580, aChanged=0x00007fff5fbf5247, aIsImportant=false, aIsSVGMode=false) + 74 at nsCSSParser.cpp:12851
  (lldb) p aPropValue
  (const nsAString_internal) $16 = u"-25px"

  (lldb) p this
  (nsHTMLDocument *) $18 = 0x0000000115b56000
  (lldb) p mContentType
  (nsCString) $19 = {
    nsACString_internal = "text/html"
  }

* nscolor

  nscolors (32-bit RGBA colors) have a type summary that shows the color as
  one of the CSS 2.1 color keywords, a six digit hex color, an rgba() color,
  or the "transparent" keyword.

  (lldb) p this
  (nsTextFrame *) $0 = 0x00000001168245e0
  (lldb) p *this->StyleColor()
  (const nsStyleColor) $1 = {
    mColor = lime
  }
  (lldb) expr -R -- *this->StyleColor()
  (const nsStyleColor) $2 = {
    mColor = 4278255360
  }

* nsIAtom

  Atoms have a type summary that shows the string value inside the atom.

  (lldb) frame info
  frame #0: 0x00000001028b8c49 XUL`mozilla::dom::Element::GetBoolAttr(this=0x0000000115ca1c50, aAttr=0x000000011012a640) const + 25 at Element.h:907
  (lldb) p aAttr
  (PermanentAtomImpl *) $1 = 0x000000011012a640 u"readonly"

* nsTArray and friends

  nsTArrays and their auto and fallible varieties have synthetic children
  for their elements.  This means when displaying them with "expr" (or "p"),
  they will be shown like regular arrays, rather than showing the mHdr and
  other fields.

  (lldb) frame info
  frame #0: 0x00000001043eb8a8 XUL`SVGTextFrame::DoGlyphPositioning(this=0x000000012f3f8778) + 248 at SVGTextFrame.cpp:4940
  (lldb) p charPositions
  (nsTArray<nsPoint>) $5 = {
    [0] = {
      mozilla::gfx::BasePoint<int, nsPoint> = {
        x = 0
        y = 816
      }
    }
    [1] = {
      mozilla::gfx::BasePoint<int, nsPoint> = {
        x = 426
        y = 816
      }
    }
    [2] = {
      mozilla::gfx::BasePoint<int, nsPoint> = {
        x = 906
        y = 816
      }
    }
  }
  (lldb) expr -R -- charPositions
  (nsTArray<nsPoint>) $4 = {
    nsTArray_Impl<nsPoint, nsTArrayInfallibleAllocator> = {
      nsTArray_base<nsTArrayInfallibleAllocator, nsTArray_CopyWithMemutils> = {
        mHdr = 0x000000012f3f1b80
      }
    }
  }

* nsTextNode, nsTextFragment

  Text nodes have a type summary that shows the nsTextFragment in the
  nsTextNode, which itself has a type summary that shows the text
  content.

  (lldb) p this
  (nsTextFrame *) $14 = 0x000000011811bb10
  (lldb) p mContent
  (nsTextNode *) $15 = 0x0000000118130110 "Search or enter address"

