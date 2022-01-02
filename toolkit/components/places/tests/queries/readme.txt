These are tests specific to the Places Query API.

We are tracking the coverage of these tests here:
http://wiki.mozilla.org/QA/TDAI/Projects/Places_Tests

When creating one of these tests, you need to update those tables so that we
know how well our test coverage is of this area.  Furthermore, when adding tests
ensure to cover live update (changing the query set) by performing the following
operations on the query set you get after running the query:
* Adding a new item to the query set
* Updating an existing item so that it matches the query set
* Change an existing item so that it does not match the query set
* Do multiple of the above inside an Update Batch transaction.
* Try these transactions in different orders.

Use the stub test to help you create a test with the proper structure.
