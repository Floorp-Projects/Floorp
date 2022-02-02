This chapter describes the API for creating and destroying condition
variables, notifying condition variables of changes in monitored data,
and making a thread wait on such notification.

-  `Condition Variable Type <#Condition_Variable_Type>`__
-  `Condition Variable Functions <#Condition_Variable_Functions>`__

Conditions are closely associated with a single monitor, which typically
consists of a mutex, one or more condition variables, and the monitored
data. The association between a condition and a monitor is established
when a condition variable is created, and the association persists for
its life. In addition, a static association exists between the condition
and some data within the monitor. This data is what will be manipulated
by the program under the protection of the monitor.

A call to ``PR_WaitCondVar`` causes a thread to block until a specified
condition variable receives notification of a change of state in its
associated monitored data. Other threads may notify the condition
variable when changes occur.

For an introduction to NSPR thread synchronization, including locks and
condition variables, see `Introduction to
NSPR <Introduction_to_NSPR>`__.

For reference information on NSPR locks, see
`Locks <NSPR_API_Reference/Locks>`__.

NSPR provides a special type, ``PRMonitor``, for use with Java. Unlike a
mutex of type ``PRLock``, which can have multiple associated condition
variables of type ``PRCondVar``, a mutex of type ``PRMonitor`` has a
single, implicitly associated condition variable. For information about
``PRMonitor``, see `Monitors <Monitors>`__.

.. _Condition_Variable_Type:

Condition Variable Type
-----------------------

-  ``PRCondVar``

.. _Condition_Variable_Functions:

Condition Variable Functions
----------------------------

-  ``PR_NewCondVar``
-  ``PR_DestroyCondVar``
-  ``PR_WaitCondVar``
-  ``PR_NotifyCondVar``
-  ``PR_NotifyAllCondVar``
