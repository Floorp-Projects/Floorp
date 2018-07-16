Again Selector
==============

When you push to try, the computed ``try_task_config.json`` is saved in a
history file under ``~/.mozbuild/history`` (note: the ``syntax`` selector does
not use ``try_task_config.json`` yet so does not save any history). You can
then use the ``again`` selector to re-push any of your previously generated
task configs.

In the simple case, you can re-run your last try push with:

.. code-block:: shell

    $ mach try again

If you want to re-push a task config a little further down the history stack,
first you need to figure out its index with:

.. code-block:: shell

    $ mach try again --list

Then run:

.. code-block:: shell

    $ mach try again --index <index>

Note that index ``0`` is always the most recent ``try_task_config.json`` in the
history. You can clear your history with:

.. code-block:: shell

    $ mach try again --purge

Only the 10 most recent pushes will be saved in your history.
