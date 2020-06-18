Integration with other software
===============================

Integration with pre-commit
---------------------------

You can integrate yamllint in `pre-commit <http://pre-commit.com/>`_ tool.
Here is an example, to add in your .pre-commit-config.yaml

.. code:: yaml

  ---
  # Update the rev variable with the release version that you want, from the yamllint repo
  # You can pass your custom .yamllint with args attribute.
  - repo: https://github.com/adrienverge/yamllint.git
    rev: v1.17.0
    hooks:
      - id: yamllint
        args: [-c=/path/to/.yamllint]
