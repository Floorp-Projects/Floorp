.. -*- Mode: rst; fill-column: 80; -*-

============================
Activity Stream UI Telemetry
============================

Building on top of UI Telemetry, Activity Stream records additional information about events and user context in which they occur.
The ``extras`` field is used for that purpose; additional information is structured as JSON blobs.

Session
=======
Activity Stream events are recorded as part of the "activitystream.1" session.

Global extras
=============
A concept of a "global" extra is meant to support recording certain context information with every event that is being sent, regardless of its type.

``fx_account_present``, values: true, false
Indicates if Firefox Account is currently enabled.

Extra information available for various event types
===================================================
Action position
---------------
Common to most recorded events is the 0-based ``action_position`` extra. For non-menu interactions it
indicates position of an item being interacted with. For example, click on a third top site of a
second page will have ``action_position = 6``, given that the first page had 4 top sites.

Additionally for Top Site interactions, 0-based ``page_number`` is also recorded.

For menu interactions, or interactions with menu items, it will indicate position of a top site or a
highlight for which the menu is being displayed.

Top Site interactions
---------------------
Three event types are recorded:

1) User clicked on a Top Site: event="loadurl.1", method="listitem"
2) User clicked on the menu button: event="show.1", method="contextmenu"
3) User swiped left/right (only for Activity Stream topsites): event="action.1", method="list", extras="swipe_forward"/"swipe_back"

For each click event (1/2), in addition to global extras, the following information is recorded:

.. code-block:: js

    extras: {
        ...
        "source_type": "topsites",
        "source_subtype": "pinned"/"suggested"/"top",
        "action_position": number, /* 0-based index of a top site being interacted with */
        "page_number": number, /* 0-based index of a page on which top site appears */
    }

Subtype indicates a reason an item which is being interacted with appeared in the Top Sites:

- "pinned": a pinned top site, specifically a non-positioned "Activity Stream pinned" site
- "suggested": a suggested top site, one of the default ones displayed when there's not enough browsing history available
- "top": a frecency-based top site, based on browsing history. Neither "pinned" nor "suggested".

Highlight interactions
----------------------
Two event types are recorded:

1) User clicked on a Highlight: event="loadurl.1", method="listitem"
2) User clicked on the menu button: event="show.1", method="contextmenu"

For both event types, in addition to global extras, the following information is recorded:

.. code-block:: js

    extras: {
        ...
        "source_type": "highlights",
        "source_subtype": "visited"/"bookmarked"
    }

Subtype indicates reason an item being which is being interacted with appeared in the Highlights:
- "visited": a website has been visited recently
- "bookmarked": a website has been bookmarked recently

For "loadurl.1" event, the following extra information is also recorded:

.. code-block:: js

    extras: {
        ...
        "action_position": number, /* 0-based index of a highlight being interacted with */
        "count": number, /* total number of highlights displayed */
    }

Context Menu interactions
-------------------------
Every interaction with a context menu item is recorded using: event="action.1", method="contextmenu"

For all interactions, in addition to global extras, the following information is recorded:

.. code-block:: js

    extras: {
        ...
        "item": string, /* name of a menu item */
        "source_type": "topsites"/"highlights",
        "source_subtype": string, /* depending on type, one of: "pinned", "suggested", "top", "visited", "bookmarked" */
        "action_position": number, /* 0-based index of a top site or highlight item which owns this menu */
    }

Possible values for "item" key (names of menu items), in no particular order:

- "share"
- "add_bookmark"
- "remove_bookmark"
- "pin"
- "unpin"
- "copy"
- "homescreen"
- "newtab" (private tab actions are collapsed into "newtab" telemetry due to our privacy guidelines)
- "dismiss"
- "delete"

Full Examples
=============
Following examples of events are here to provide a better feel for the overall shape of telemetry data being recorded.

1) User with an active Firefox Account clicked on a menu item for a third highlight ("visited"):
    ::

        session="activitystream.1"
        event="show.1"
        method="contextmenu"
        extras="{
            'fx_account_present': true,
            'source_type': 'highlights',
            'source_subtype': 'visited',
            'action_position': 2
        }"

2) User with no active Firefox Account clicked on a second highlight (recent bookmark), with total of 7 highlights being displayed:
    ::

        session="activitystream.1"
        event="loadurl.1"
        method="listitem"
        extras="{
            'fx_account_present': false,
            'source_type': 'highlights',
            'source_subtype': 'bookmarked'
            'action_position': 1,
            'count': 7
        }"

3) User with an active Firefox Account clicked on a third pinned top site:
    ::

        session="activitystream.1"
        event="loadurl.1"
        method="listitem"
        extras="{
            'fx_account_present': true,
            'source_type': 'topsites',
            'source_subtype': 'pinned',
            'action_position': 2,
            'page_number': 0
        }"

4) User with an active Firefox Account clicked on a "share" context menu item, which was displayed for a regular top site number 6:
    ::

        session="activitystream.1"
        event="action.1"
        method="contextmenu"
        extras="{
            'fx_account_present': true,
            'source_type': 'topsites',
            'source_subtype': 'top',
            'item': 'share',
            'action_position': 5
        }"
