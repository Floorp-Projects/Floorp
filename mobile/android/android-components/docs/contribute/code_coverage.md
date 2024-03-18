---
layout: page
title: Code coverage
permalink: /contributing/code-coverage
---

# Code Coverage

> In computer science, test coverage is a measure used to describe the degree to which the source code of a program is executed when a particular test suite runs. A program with high test coverage, measured as a percentage, has had more of its source code executed during testing, which suggests it has a lower chance of containing undetected software bugs compared to a program with low test coverage. ([Wikipedia](https://en.wikipedia.org/wiki/Code_coverage))

# Automated reports

For pull requests and main pushes we generate code coverage reports on taskcluster and they can be accessed via the GitHub checks.

# Generating reports locally

Locally you can generate a coverage report for a module with the following command:
```bash
./gradlew -Pcoverage <module>:build
```

After that you'll find an HTML report at the following location:
```
components/<path to module>/build/reports/jacoco/jacocoTestReport/html/index.html
```
