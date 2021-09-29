==========================
Template for Mentored Bugs
==========================

We like to encourage external contributions to the Firefox code base and the Telemetry module specifically.
In order to set up a mentored bug, you can use the following template.
Post it as a comment and add relevant steps in part 3.

.. code-block:: md

   To help Mozilla out with this bug, here's the steps:

   1. Comment here on the bug that you want to volunteer to help.
      This will tell others that you're working on the next steps.
   2. [Download and build the Firefox source code](https://firefox-source-docs.mozilla.org/setup/index.html)
      * If you have any problems, please ask on
        [Element/Matrix](https://chat.mozilla.org/#/room/#introduction:mozilla.org)
        in the `#introduction` channel. They're there to help you get started.
      * You can also read the
        [Firefox Contributors' Quick Reference](https://firefox-source-docs.mozilla.org/contributing/contribution_quickref.html),
        which has answers to most development questions.
   3. Start working on this bug. <SPECIFIC STEPS RELEVANT TO THIS BUG>
      * If you have any problems with this bug,
        please comment on this bug and set the needinfo flag for me.
        Also, you can find me and my teammates on the `#telemetry` channel on
        [Element/Matrix](https://chat.mozilla.org/#/room/#telemetry:mozilla.org)
        most hours of most days.
   4. Build your change with `mach build` and test your change with
      `mach test toolkit/components/telemetry/tests/`.
      Also check your changes for adherence to our style guidelines by using `mach lint`
   5. Submit the patch (including an automated test, if applicable) for review.
      Mark me as a reviewer so I'll get an email to come look at your code.
      * [Getting your code reviewed](https://firefox-source-docs.mozilla.org/setup/contributing_code.html#getting-your-code-reviewed)
      * This is when the bug will be assigned to you.
   6. After a series of reviews and changes to your patch,
      I'll mark it for checkin or push it to autoland.
      Your code will soon be shipping to Firefox users worldwide!
   7. ...now you get to think about what kind of bug you'd like to work on next.
      Let me know what you're interested in and I can help you find your next contribution.


Don't forget to add yourself as a :code:`Mentor` on the bug,
and add these tags to the :code:`Whiteboard`:

* Add :code:`[lang=<language>]` to show what languages solving this bug will involve.
* Add one of :code:`[good first bug]`, :code:`[good second bug]`, :code:`[good next bug]`
  to indicate for whom this bug might be a good point for contribution.

If this is a Good First Bug, be sure to also add the :code:`good-first-bug` :code:`Keyword`.
