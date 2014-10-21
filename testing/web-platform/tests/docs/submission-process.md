Test submission is via the typical GitHub workflow.

* Fork the [GitHub repository][repo] (and make sure you're still relatively in
sync with it if you forked a while ago)

* Create a branch for your changes. Being a key of effective Git flow, it is
strongly recommended that the **topic branch** tradition be followed here,
i.e. the branch naming convention is based on the "topic" you will be working
on, e.g. `git checkout -b topic-name`

* Make your changes

* Push that to your GitHub repository

* Send in a pull request based on the above

* When you get review comments, make more commits to your branch to
  address the comments (**note**: Do *not* rewrite existing commits using
  e.g. `git commit --amend` or `git rebase -i`. The review system
  depends on the full branch history).

* Once everything is reviewed and all issues are addressed, your pull
  request will be automatically merged.

For detailed guidelines on setup and each of these steps, please refer to the
[Github Test Submission][github101] documentation.

Hop on to [irc or the mailing list][discuss] if you have an
issue. There is no need to announce your review request, as soon as
you make a Pull Request GitHub will inform interested parties.

[repo]: https://github.com/w3c/web-platform-tests/
[github101]: ./github-101.html
[discuss]: /discuss.html
