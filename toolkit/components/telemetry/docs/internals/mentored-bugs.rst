==========================
Template for Mentored Bugs
==========================

We like to encourage external contributions to the Firefox code base and the Telemetry module specifically.
In order to set up a mentored bug, you can use the following template.
Post it as a comment and add relevant steps in part 3.

.. code-block:: md

   To help Mozilla out with this bug, here's the steps:

   1. Comment here on the bug that you want to volunteer to help. I (or someone else) will assign it to you.
   2. [Download and build the Firefox source code](https://developer.mozilla.org/docs/Mozilla/Developer_guide/Build_Instructions/Simple_Firefox_build)
      * If you have any problems, please ask on [Riot/Matrix](https://chat.mozilla.org/#/room/#introduction:mozilla.org) in the `introduction` channel. They're there to help you get started.
      * You can also read the [Developer Guide](https://developer.mozilla.org/docs/Mozilla/Developer_guide/Introduction), which has answers to most development questions:
   3. Start working on this bug. <SPECIFIC STEPS RELEVANT TO THIS BUG>
      * If you have any problems with this bug, please comment on this bug and set the needinfo flag for me. Also, you can find me and my teammates on the #telemetry channel on [IRC](https://wiki.mozilla.org/Irc) most hours of most days.
   4. Build your change with `mach build` and test your change with `mach test toolkit/components/telemetry/tests/`. Also check your changes for adherence to our style guidelines by using `mach lint`
   5. Submit the patch (including an automated test, if applicable) for review. Mark me as a reviewer so I'll get an email to come look at your code.
      * [How to Submit a Patch](https://developer.mozilla.org/docs/Mozilla/Developer_guide/How_to_Submit_a_Patch)
   6. After a series of reviews and changes to your patch, I'll mark it for checkin or push it to autoland. Your code will soon be shipping to Firefox users worldwide!
   7. ...now you get to think about what kind of bug you'd like to work on next. Let me know what you're interested in and I can help you find your next contribution.


Don't forget to add a Mentor and add tags to the whiteboard.

* Add :code:`[lang=<language>]` to show what languages solving this bug will involve.
* Add one of :code:`[good first bug]`, :code:`[good second bug]`, :code:`[good next bug]` to indicate for whom this bug might be a good point for contribution.
