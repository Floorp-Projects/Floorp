.. _components/pictureinpicture:

==================
Picture-in-Picture
==================

This component makes it possible for a ``<video>`` element on a web page to be played within
an always-on-top video player.

This documentation covers the architecture and inner workings of both the mechanism that
displays the ``<video>`` in the always-on-top video player, as well as the mechanism that
displays the Picture-in-Picture toggle that overlays ``<video>`` elements, which is the primary
method for launching the feature.


High-level overview
===================

The following diagram tries to illustrate the subcomponents, and how they interact with one another.

.. image:: PiP-diagram.svg

Let's suppose that the user has loaded a document with a ``<video>`` in it, and they decide to open
it in a Picture-in-Picture window. What happens?

First the ``PictureInPictureToggleChild`` component notices when ``<video>`` elements are added to the
DOM, and monitors the mouse as it moves around the document. Once the mouse intersects a ``<video>``,
``PictureInPictureToggleChild`` causes the Picture-in-Picture toggle to appear on that element.

If the user clicks on that toggle, then the ``PictureInPictureToggleChild`` dispatches a chrome-only
``MozTogglePictureInPicture`` event on the video, which is handled by the ``PictureInPictureLauncherChild`` actor
for that document. The reason for the indirection via the event is that the media context menu can also
trigger Picture-in-Picture by dispatching the same event on the video. Upon handling the event, the
``PictureInPictureLauncherChild`` actor then sends a ``PictureInPicture:Request`` message to the parent process.
The parent process opens up the always-on-top player window, with a remote ``<xul:browser>`` that runs in
the same content process as the original ``<video>``. The parent then sends a message to the player
window's remote ``<xul:browser>`` loaded in the player window. A ``PictureInPictureChild`` actor
is instantiated for the empty document loaded inside of the player window browser. This
``PictureInPictureChild`` actor constructs its own ``<video>`` element, and then tells Gecko to clone the
frames from the original ``<video>`` to the newly created ``<video>``.

At this point, the video is displaying in the Picture-in-Picture player window.

Next, we'll discuss the individual subcomponents, and how they operate at a more detailed level.


The Picture-in-Picture toggle
=============================

One of the primary challenges faced when developing this feature was the fact that, in practice, mouse
events tend not to reach ``<video>`` elements. This is usually because the ``<video>`` element is
contained within a hierarchy of other DOM elements that are capturing and handling any events that
come down. This often occurs on sites that construct their own video controls. This is why we cannot
simply use a ``mouseover`` event handler on the ``<video>`` UAWidget - on sites that do the event
capturing, we'll never receive those events and the toggle will not be accessible.

Other times, the problem is that the video is overlaid with a semi or fully transparent element
which captures any mouse events that would normally be dispatched to the underlying ``<video>``.
This can occur, for example, on sites that want to display an overlay when the video is paused.

To work around this problem, the `PictureInPictureToggleChild` actor class samples the latest
``mousemove`` event every ``MOUSEMOVE_PROCESSING_DELAY_MS`` milliseconds, and then calls
``nsIDOMWindowUtils.nodesFromRect`` with the ``aOnlyVisible`` argument to get the full
list of visible nodes that exist underneath a 1x1 rect positioned at the mouse cursor.

If a ``<video>`` is in that list, then we reach into its shadow root, and update some
attributes to tell it to maybe show the toggle.

The underlying ``UAWidget`` for the video is defined in ``videocontrols.js``, and ultimately
chooses whether or not to display the toggle based on the following heuristics:

1. Is the video less than 45 seconds?
2. Is either the width or the height of the video less than 160px?
3. Is the video silent?

If any of the above is true, the underlying ``UAWidget`` will hide the toggle, since it's
unlikely that the user will want to pop the video out into an always-on-top player window.


Video registration
==================

Sampling the latest ``mousemove`` event every ``MOUSEMOVE_PROCESSING_DELAY_MS`` is not free,
computationally speaking, so we only do this if there are one or more ``<video>`` elements
visible on the page. We use an ``IntersectionObserver`` to notice when there is a ``<video>``
within the viewport, and if there are 1 or more ``<video>`` elements visible, then we start
sampling the ``mousemove`` event.

Videos are added to the ``IntersectionObserver`` when they are added to the DOM by listening
for the ``UAWidgetSetupOrChange`` event. This is considered being "registered".


``docState``
============

``PictureInPictureChild.jsm`` contains a ``WeakMap`` mapping ``document``'s to various information
that ``PictureInPictureToggleChild`` wants to retain for the lifetime of that ``document``. For
example, whether or not we're in the midst of handling the user clicking down on their pointer
device. Any state that needs to be remembered should be added to the ``docState`` ``WeakMap``.


Clicking on the toggle
======================

If the user clicks on the Picture-in-Picture toggle, we don't want the underlying webpage to
know that this happened, since this could result in unexpected behaviour, like a page
navigation (for example, if the ``<video>`` is a long-running advertisement that navigates
upon click).

To accomplish this, we listen for all events fired on a mouse click on the root window during
the capturing phase. This allows us to handle the events before they are dispatched to content.

The first event that is fired, ``pointerdown``, is captured, and we check the ``docState`` to see
whether or not we're showing a toggle on any videos. If so, we check the coordinates of that
toggle against the coordinates of the ``pointerdown`` event to determine if the user is clicking
on the toggle. If so, we set a flag in the ``docState`` so that any subsequent events from the
click (like ``mousedown``, ``mouseup``, ``pointerup``, ``click``) are captured and suppressed.
If the ``pointerdown`` event didn't occur within a toggle, we let the events pass through as
normal.

If we determine that the click has occurred on the toggle, a ``MozTogglePictureInPicture`` event
is dispatched on the underlying ``<video>``. This event is handled by the separate
``PictureInPictureLauncherChild`` class.

PictureInPictureLauncherChild
=============================

A small actor class whose only responsibility is to tell the parent process to open an always-on-top-window by sending a ``PictureInPicture:Request`` message to its parent actor.

Currently, this only occurs when a chrome-only ``MozTogglePictureInPicture`` event is dispatched by the ``PictureInPictureToggleChild`` when the user clicks the Picture-in-Picture toggle button
or uses the context-menu.

PictureInPictureChild
=====================

The ``PictureInPictureChild`` actor class will run in a content process containing a video, and is instantiated when the player window's `player.js` script runs its initialization. A ``PictureInPictureChild`` maps an individual ``<video>``
to a player window instance. It creates an always-on-top window, and sets up a new ``<video>`` inside of this window to clone frames from another ``<video>``
(which will be in the same process, and have its own ``PictureInPictureChild``). Creating this window also causes the new ``PictureInPictureChild`` to be created.
This instance will monitor the originating ``<video>`` for changes, and to receive commands from the player window if the user wants to control the ``<video>``.

PictureInPicture.jsm
====================

This module runs in the parent process, and is also the scope where all ``PictureInPictureParent`` instances reside. ``PictureInPicture.jsm``'s job is to send and receive messages from ``PictureInPictureChild`` instances, and to react appropriately.

Critically, ``PictureInPicture.jsm`` is responsible for opening up the always-on-top player window, and passing the relevant information about the ``<video>`` to be displayed to it.


The Picture-in-Picture player window
====================================

The Picture-in-Picture player window is a chrome-privileged window that loads an XHTML document. That document contains a remote ``<browser>`` element which is repurposed during window initialization to load in the same content process as the originating ``<video>``.

The player window is where the player controls are defined, like "Play" and "Pause". When the user interacts with the player controls, a message is sent down to the appropriate ``PictureInPictureChild`` to call the appropriate method on the underlying ``<video>`` element in the originating tab.


Cloning the video frames
========================

While it appears as if the video is moving from the original ``<video>`` element to the player window, what's actually occurring is that the video frames are being *cloned* to the player window ``<video>`` element. This cloning is done at the platform level using a privileged method on the ``<video>`` element: ``cloneElementVisually``.


``cloneElementVisually``
------------------------

.. code-block:: js

    Promise<void> video.cloneElementVisually(otherVideo);

This will clone the frames being decoded for ``video`` and display them on the ``otherVideo`` element as well. The returned Promise resolves once the cloning has successfully started.


``stopCloningElementVisually``
------------------------------

.. code-block:: js

    void video.stopCloningElementVisually();

If ``video`` is being cloned visually to another element, calling this method will stop the cloning.


``isCloningElementVisually``
----------------------------

.. code-block:: js

    boolean video.isCloningElementVisually;

A read-only value that returns ``true`` if ``video`` is being cloned visually.

API References
====================================
.. toctree::
   :maxdepth: 1

   picture-in-picture-api
   player-api

