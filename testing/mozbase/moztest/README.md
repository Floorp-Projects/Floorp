# Moztest

Package for handling Mozilla test results.


## Usage example

This shows how you can create an xUnit representation of python unittest results.

    from results import TestResultCollection
    from output import XUnitOutput

    collection = TestResultCollection.from_unittest_results(results)
    out = XUnitOutput()
    with open('out.xml', 'w') as f:
        out.serialize(collection, f)
