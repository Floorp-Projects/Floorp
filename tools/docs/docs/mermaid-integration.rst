Mermaid Integration
===================

Mermaid is a tool that lets you generate flow charts, sequence diagrams, gantt charts, class diagrams and vcs graphs from a simple markup language. This
means you can embed these directly in the source rather than creating an image
with some external tool and then checking it into the tree. To add a diagram,
simply put something like this into your page:

.. code-block:: shell

    .. mermaid::

        graph TD;
            A-->B;
            A-->C;
            B-->D;
            C-->D;


See `mermaid's official <https://mermaid-js.github.io/mermaid/#/>`__ docs for more details on the syntax.
