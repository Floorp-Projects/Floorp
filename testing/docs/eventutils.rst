EventUtils documentation
========================

``EventUtils``' methods are available in all browser mochitests on the ``EventUtils``
object.

In mochitest-plain and mochitest-chrome, you can load
``"chrome://mochikit/content/tests/SimpleTest/EventUtils.js"`` using a regular
HTML script tag to gain access to this set of utilities. In this case, all the
documented methods here are **not** on a separate object, but available as global
functions.

Mouse input
-----------

.. js:autofunction:: sendMouseEvent
.. js:autofunction:: EventUtils.synthesizeMouse
.. js:autofunction:: synthesizeMouseAtCenter
.. js:autofunction:: synthesizeNativeMouseEvent
.. js:autofunction:: synthesizeMouseExpectEvent

.. js:autofunction:: synthesizeWheel
.. js:autofunction:: EventUtils.synthesizeWheelAtPoint
.. js:autofunction:: sendWheelAndPaint
.. js:autofunction:: sendWheelAndPaintNoFlush

Keyboard input
--------------

.. js:autofunction:: sendKey
.. js:autofunction:: EventUtils.sendChar
.. js:autofunction:: sendString
.. js:autofunction:: EventUtils.synthesizeKey
.. js:autofunction:: synthesizeNativeKey
.. js:autofunction:: synthesizeKeyExpectEvent

Drag and drop
-------------

.. js:autofunction:: synthesizeDragOver
.. js:autofunction:: synthesizeDrop
.. js:autofunction:: synthesizeDropAfterDragOver
.. js:autofunction:: synthesizePlainDragAndDrop
.. js:autofunction:: synthesizePlainDragAndCancel
.. js:autofunction:: sendDragEvent
