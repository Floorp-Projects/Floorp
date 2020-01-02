Static analyzers and linters
============================

Because Firefox is a complex piece of software, a lot of tools are
executed to identify issues at development phase.
In this document, we try to list these all tools.

.. list-table:: C/C++
   :widths: 25 25 25 20
   :header-rows: 1

   * - Tools
     - Has autofixes
     - More info
     - Upstream
   * - Custom clang checker
     -
     - `Source <https://searchfox.org/mozilla-central/source/build/clang-plugin>`_
     -
   * - Clang-Tidy
     - Yes
     - :ref:`Static analysis <Mach static analysis>`
     - https://clang.llvm.org/extra/clang-tidy/checks/list.html
   * - Clang analyzer
     -
     -
     - https://clang-analyzer.llvm.org/
   * - Coverity
     -
     -
     -
   * - cpp virtual final
     -
     - :ref:`cpp virtual final`
     -
   * - Semmle/LGTM
     -
     -
     -
   * - clang-format
     - Yes
     - :ref:`Formatting C++ Code With clang-format`
     - https://clang.llvm.org/docs/ClangFormat.html

.. list-table:: JavaScript
   :widths: 25 25 25 25
   :header-rows: 1

   * - Tools
     - Has autofixes
     - More info
     - Upstream
   * - Eslint
     - Yes
     - :ref:`ESLint`
     - https://eslint.org/
   * - Mozilla ESLint
     -
     - :ref:`Mozilla ESLint Plugin`
     -
   * - Prettier
     - Yes
     - :ref:`JavaScript Coding style`
     - https://prettier.io/



.. list-table:: Python
   :widths: 25 25 25 25
   :header-rows: 1

   * - Tools
     - Has autofixes
     - More info
     - Upstream
   * - Flake8
     - Yes (with `autopep8 <https://github.com/hhatto/autopep8>`_)
     -
     - http://flake8.pycqa.org/
   * - Python 2/3 compatibility check
     -
     - :ref:`Python 2/3 compatibility check`
     -


.. list-table:: Rust
   :widths: 25 25 25 25
   :header-rows: 1

   * - Tools
     - Has autofixes
     - More info
     - Upstream
   * - Rustfmt
     - Yes
     -
     - https://github.com/rust-lang/rustfmt
   * - Clippy
     -
     - :ref:`clippy`
     - https://github.com/rust-lang/rust-clippy

.. list-table:: Java
   :widths: 25 25 25 25
   :header-rows: 1

   * - Tools
     - Has autofixes
     - More info
     - Upstream
   * - Infer
     -
     -
     - https://github.com/facebook/infer

