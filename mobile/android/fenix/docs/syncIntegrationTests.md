### Sync Integration Tests
The aim of these tests is to check that the synchronization is working between Fenix and Desktop. The intention is to add tests for History, Bookmarks, Tabs and Logins.
At this moment only tests for History and Bookmarks are defined.

### Steps to Run
To run these tests you will need Python 2 and pipenv installed. Once you have these, make sure you're in the `syncintegration` directory and run the following:

`$ pipenv install`
`$ pipenv run pytest`

When a test is launched a stage account is created. That will be used both in Desktop and Fenix to be sure that what is saved in one place is shown in the other.

The process for example for History item Desktop -> Fenix, would be:
- Desktop is launched, user signed in and history item created.
- Android sim is launched (Pixel 3 API28), Fenix app starts and same user is signed in, then we go to History list and verify that the item is there.


### Results
Due to the set up necessary these tests do not run as part of the regular CI, via Taskcluster.
The idea is to have them running on Jenkins periodically (TBD how often).
Once they finish there is a slack notificattion received informing about the result (so far that is configured for #firefox-ios-alerts)

A html file is generated with all the info, for each step to make it easy to debug in case of failure.

## Notes
More detailed info can be found [`here`](https://docs.google.com/document/d/1dhxlbGQBA6aJi2Xz-CsJZuGJPRReoL7nfm9cYu4HcZI/edit?usp=sharing)
