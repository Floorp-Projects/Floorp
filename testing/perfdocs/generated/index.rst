###################
Performance Testing
###################

.. toctree::
  :maxdepth: 2
  :hidden:
  :glob:

  DAMP
  awsy
  fxrecord
  mozperftest
  raptor
  talos

Performance tests are designed to catch performance regressions before they reach our
end users. At this time, there is no unified approach for these types of tests,
but `mozperftest </testing/perfdocs/mozperftest.html>`_ aims to provide this in the future.

For more detailed information about each test suite, see each projects' documentation:

  * :doc:`DAMP`
  * :doc:`awsy`
  * :doc:`fxrecord`
  * :doc:`mozperftest`
  * :doc:`raptor`
  * :doc:`talos`


Here are the active PerfTest components/modules and their respective owners:

    * AWFY (Are We Fast Yet) -
        - Owner: Beatrice A.
        - Description: A public dashboard comparing Firefox and Chrome performance metrics
    * AWSY (Are We Slim Yet)
        - Owner: Alexandru I.
        - Description: Project that tracks memory usage across builds
    * Raptor
        - Owner: Sparky
        - Co-owner: Kash
        - Description: Test harness that uses Browsertime (based on webdriver) as the underlying engine to run performance tests
    * CondProf (Conditioned profiles)
        - Owner: Sparky
        - Co-owner: Jmaher
        - Description: Provides tooling to build, and obtain profiles that are preconditioned in some way.
    * fxrecord
        - Owner: Sparky
        - Co-owners: Kash, Andrej
        - Description: Tool for measuring startup performance for Firefox Desktop
    * Infrastructure
        - Owner: Sparky
        - Co-owners: Kash, Andrej
        - Description: All things involving: TaskCluster, Youtube Playback, Bitbar, Mobile Configs, etc...
    * Mozperftest
        - Owner: Sparky
        - Co-owners: Kash, Andrej
        - Description: Testing framework used to run performance tests
    * Mozperftest Tools
        - Owner: Sparky
        - Co-owner: Alexandru I.
        - Description: Various tools used by performance testing team
    * Mozproxy
        - Owner:  Sparky
        - Co-owner: Kash
        - Description: An http proxy used to run tests against third-party websites in a reliable and reproducible way
    * PerfCompare
        - Owner: Kimberly
        - Co-owner: Beatrice A.
        - Description: Performance comparison tool used to compare performance of different commits within a repository
    * PerfDocs
        - Owner: Sparky
        - Co-owner: Alexandru I.
        - Description: Automatically generated performance test engineering documentation
    * PerfHerder
        - Owner: Beatrice A
        - Co-owner: Andra A.
        - Description: The framework used by the performance sheriffs to find performance regressions and for storing, and visualizing our performance data.
    * Performance Sheriffing
        - Owner: Alexandru I.
        - Co-owner: Andra A.
        - Description: Performance sheriffs are responsible for finding commits that cause performance regressions and getting fixes from devs or backing out the changes
    * Talos
        - Owner: Sparky
        - Co-owner: Andrej
        - Description: Testing framework used to run Firefox-specific performance tests
    * WebPageTest
        - Owner: Andrej
        - Co-owner: Sparky
        - Description: A test running in the mozperftest framework used as a third party performance benchmark

You can additionally reach out to our team on
the `#perftest <https://matrix.to/#/#perftest:mozilla.org>`_ channel on matrix

For more information about the performance testing team,
`visit the wiki page <https://wiki.mozilla.org/TestEngineering/Performance>`_.
