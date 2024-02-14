# Feature & Issue workflow

## High-Level Steps

| Issue opened| ➡️ |Triaged| ➡️ |Ready in/for Backlog| ➡️ | Ready for UX | ➡️ | Ready for Eng| ➡️ |Eng done| ➡️ |Ready for review| ➡️ |Ready for QA| ➡️ |Done
|------|------|------|------|------|------|------|------|------|------|------|------|------|------|------|------|------

## Details

**Issue opened**
- Issue is created (by anybody)
- Ready for triage, where the issue gets either closed, commented on, left in triage for more info, assigned labels, and milestones

**Triaged**
- Is triaged, which means issue has a P label, has a milestone assigned (can be backlog)
- Issue if possible can already get estimation, T-shirt size

**Ready in/for Backlog**
- Issue is in Backlog milestone
- Issue includes enough information (follows template) that allows team members to estimate, understand the scope, user benefit, the what, and acceptance criteria
- Request for probes/KPIs reviewed and approved by product and data analyst
- Once estimated, it's ready to be moved into a sprint

**Ready for UX**
- When UX picks up issue, assigns themselves to the issue
- UX to provide mocks, attach to Github issue
- Once UX is done, ready for eng, UX resource to unassign themselves, and use  "ready for eng breakdown" label
- Can be skipped if no UX is needed

**Ready for Eng**
- Eng should only pick up issues that are ready for Eng and assigned to a sprint/milestone (not Backlog)
- Copy/content strategist has provided strings

**Eng done**
- Issue is eng done only if PR was submitted AND PR was reviewed
- PR is closed but not issue
- Issue will be assigned by eng to product manager (PM assigns it to other stakeholders where appropriate)
- Strings exported

**Ready for Review**
- During sprints: Once PR is closed, the next day Nightly should have the changes and stakeholders can verify
- During sprint review: everyone can review, comment
- Once stakeholders(Marketing, PM) are happy with result, PM assigns "ready for QA" label

**Ready for QA**
- QA to watch for issues with "ready for QA" label AND in current sprint or major milestone
- QA to assign the ticket they are currently working on to themselves
- Once QA verified and completed issue, un-assign themselves from issue
- Outline and record test steps in TestRail
- Identify new test(s) for automation and create github issue with "automation" label
- Closes issue

**Done**
- Issue is closed, no assignees and assigned sprint or major milestone
