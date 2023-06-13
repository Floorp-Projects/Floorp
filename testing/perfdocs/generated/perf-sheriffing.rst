======================
Performance Sheriffing
======================

.. contents::
   :depth: 2
   :local:

Overview:
*********

Performance sheriffs are responsible to make sure that performance changes in Firefox are detected
and dealt with. They look at data and performance metrics produced by the performance testing frameworks
and find regression, determine the root cause and get bugs on file to track all issues. The workflow we
follow is shown below in our flowchart.

Flowchart:
==========

.. image:: ./flowchart.png
   :alt: Sheriffing Workflow Flowchart
   :scale: 75%
   :align: center

The tdlr; workflow of a sheriff is backfilling jobs to get the data, investigating that data, filing
bugs/linking improvements based on the data, and following up with developers if needed.

Contacts and the Team:
======================
In the event that you have an urgent issue and need help what can you do?

If you have a question about a bug that was filed and assigned to you reach out to the sheriff who
filed the bug on matrix. If a performance sheriff is not responsive or you have a question about a bug
send a message to the `performance sheriffs matrix channel <https://chat.mozilla.org/#/room/#perfsheriffs:mozilla.org>`_
and tag the sheriff. If you still have no-one responding you can message any of the following people directly
on slack or matrix:

- `@afinder <https://people.mozilla.org/p/afinder>`_
- `@alexandrui <https://people.mozilla.org/p/alexandrui>`_
- `@andra <https://people.mozilla.org/p/andraesanu>`_
- `@andrej <https://people.mozilla.org/p/andrej>`_
- `@beatrice <https://people.mozilla.org/p/bacasandrei>`_
- `@sparky <https://people.mozilla.org/p/sparky>`_ (reach out to only if all others unreachable)

All of the team is in EET (Eastern European Time) except for @andrej and @sparky who are in EST (Eastern Standard Time).

Regression vs Improvement:
==========================
Whenever we get a performance change we classify it as one of two things, either a regression or an improvement.
An improvement makes the performance better, while a regression makes something perform worse.
