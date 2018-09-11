---
name: "\U0001F469‍\U0001F52C A/B Experiment Request"
about: Template to run and define an A/B experiment

---

## Meta Data
(optional / if needed or relevant)
Links to past feature documents. Past issues/bugs that might provide additional context.
Links to dashboards or metrics for which the following document will be based on.

## Problem Summary
Talk about current goals of the system and how they aren't being met. What outcomes are we not delivering? This can also include an explicit request for improvement that doesn't dictate a specific or concrete solution.

## What user problem(s) are we trying to resolve?

[List of prioritized user stories using the syntax “So that … as a … I want ….”]

### Assumptions (optional)
This is where you talk about what you assume to be true. This could include assumptions around what users are doing, errors, gaps, etc., based on anecdotes, opinions, testing, or data.

## Outcomes
What are the outcomes you want to achieve? What is the success criteria?

## Hypothesis
A high level hypothesis of how the feature you're proposing is going help us achieve the outcomes listed above. I recommend this be a sentence of the form:

If we (do this/build this/create this experiment) for (these users), then we will see (this outcome) because we have observed (this) via (data source, UR, survey).

## Metrics

- What is the primary success metric to confirm hypothesis?
- Are there secondary success metrics which could potentially prevent this feature from rolling out?
- How will these metrics be measured? (tool, data source, visualization, etc.) Make sure to confirm that these are confirmed before releasing.

Please provide sample artifact graphs here.


## Detailed design
This is the bulk of the RFC.  Explain in enough detail to try to make it readable to someone outside of the team (other PMs, executives, etc) or for someone joining the team.
An additional goal is to reduce any doubt within the interpretation of metrics we might collect.This should get into specifics and corner-cases, and include examples of how the feature is used.

### Original Version (Present Day)
What is current situation in regards to this feature? How does it currently work? No need to go in as much detail as the suggested change but just enough to provide contrast and more context. Screenshots and user flow can often be enough.
[Add screenshots]

### Variation A
Provide details of change. If this is one of multiple variations, why do we think this change will make the better improvement. Include:
Screenshots with appropriate explanation
User flow

### Variation B (if necessary)
(Same details as variation A)

## Hypothetical Implementation Plan

Unresolved questions and risks (optional)
What parts of the design are still TBD?


## Results
- If we are developing a hypothesis and defining success metrics, we need to log them here.
- If metrics leave room to interpretation, define them. (e.g. when are they tracked, how, etc)
- Include screenshots of result graphs or data tables
- These results will likely help us develop new hypotheses.


## Conclusion
Was our hypothesis true or false and why?
Our hypothesis was true because we observed... [e.g. a 15% increase in account signup completions].

We should also address secondary metrics here:
We also observed during this test that…  [e.g. we had an increase in single device signups]

## Next Steps
There no point having a conclusion if you don’t have take-aways with next steps.

Are we releasing? Are we making changes?
