The existing preservation fixture has four cases. The new development flows
fixture has three: A-B-A selection/visibility, native TabOpen attribution and
native tab context-menu movement. `groups.json` fixes their exact ordered names.

Use `workspace_extended_only=true` in the existing Browser Integration workflow.
It runs both groups once, with separate fresh synthetic profiles and receipts
under `_dist/workspace-expanded-development/{preservation,flows}`. Missing,
duplicate, skipped, foreign, failed or later-attempt evidence rejects a PASS.
Each group retains its first case error and raw bounded logs, SHA/revision,
Runtime asset/version/BuildID, control run ID and owned process/profile cleanup.
Both limited inputs together reject before native execution. Default PR/native
jobs are preserved. The original six-only adapter and its exact twelve-event
contract are unchanged; expanded evidence cannot satisfy that contract.

The separate receipt schema is `qa3-workspace-expanded-development-v1` and any
success is only `PASS_DEVELOPMENT_ONLY`. Native/compile product qualification
remains unverified and publication unauthorized. Final qualification still needs
its separate completed producer, signed target bytes, final QA, approval, fresh
evidence revalidation and publication, without parent-run completion circularity.
