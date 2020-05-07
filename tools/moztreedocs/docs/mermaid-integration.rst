Mermaid Integration
===================

Mermaid is a tool that lets you generate flow charts, sequence diagrams, gantt
charts, class diagrams and vcs graphs from a simple markup language. This
allows charts and diagrams to be embedded and edited directly in the
documentation source files rather than creating them as images using some
external tool and checking the images into the tree.

To add a diagram, simply put something like this into your page:

.. These two examples come from the upstream website (https://mermaid-js.github.io/mermaid/#/)

.. code-block:: shell

    .. mermaid::

        graph TD;
            A-->B;
            A-->C;
            B-->D;
            C-->D;

The result will be:

.. mermaid::

     graph TD;
         A-->B;
         A-->C;
         B-->D;
         C-->D;

Or

.. code-block:: shell

    .. mermaid::

         sequenceDiagram
             participant Alice
             participant Bob
             Alice->>John: Hello John, how are you?
             loop Healthcheck
                 John->>John: Fight against hypochondria
             end
             Note right of John: Rational thoughts <br/>prevail!
             John-->>Alice: Great!
             John->>Bob: How about you?
             Bob-->>John: Jolly good!

will show:

.. mermaid::

     sequenceDiagram
         participant Alice
         participant Bob
         Alice->>John: Hello John, how are you?
         loop Healthcheck
             John->>John: Fight against hypochondria
         end
         Note right of John: Rational thoughts <br/>prevail!
         John-->>Alice: Great!
         John->>Bob: How about you?
         Bob-->>John: Jolly good!


See `Mermaid's official <https://mermaid-js.github.io/mermaid/#/>`__ docs for
more details on the syntax, and use the
`Mermaid Live Editor <https://mermaidjs.github.io/mermaid-live-editor>`__ to
experiment with creating your own diagrams.
