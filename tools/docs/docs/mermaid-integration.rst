Mermaid Integration
===================

Mermaid is a tool that lets you generate flow charts, sequence diagrams, gantt charts, class diagrams and vcs graphs from a simple markup language. This
means you can embed these directly in the source rather than creating an image
with some external tool and then checking it into the tree. To add a diagram,
simply put something like this into your page:

.. These two examples are coming from the upstream website (https://mermaid-js.github.io/mermaid/#/)

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


See `mermaid's official <https://mermaid-js.github.io/mermaid/#/>`__ docs for more details on the syntax.
