// SPDX-License-Identifier: MPL-2.0
// Read-only intake. No deployment approval or publication API is called here.
/** @typedef {{id:number, repository:{full_name:string}, path:string,
 * head_sha:string, event:string, head_branch:string, status:string,
 * conclusion:string|null}} PackageRun */
/** @typedef {{name:string, status:string, conclusion:string|null}} PackageJob */
/** @typedef {{environment:{id:number,name:string},
 * reviewers:Array<{type:string,reviewer:{id:number}}>}} PendingDeployment */
/** @typedef {{draft:boolean,prerelease:boolean,target_commitish:string}} PackageRelease */
/** @typedef {{id:number,name:string,expired:boolean,digest:string|null,
 * workflow_run:{id:number,head_sha:string}}} BundleArtifact */
/** @typedef {{run:PackageRun, headSha:string, jobs:PackageJob[],
 * prePublication:boolean, unsigned:boolean, pendingDeployments:PendingDeployment[],
 * release:PackageRelease|null, bundleArtifacts:BundleArtifact[]}} FinalSourceInput */
const TARGETS = [
  "windows/x86_64",
  "linux/x86_64",
  "linuxAarch64/aarch64",
  "mac/x86_64",
  "mac/aarch64",
];
/** @param {boolean} condition @param {string} message */
function requireCondition(condition, message) {
  if (!condition) throw new Error(message);
}
/** @param {PackageJob[]} jobs @param {string} suffix */
function successfulJob(jobs, suffix) {
  const matches = jobs.filter((job) => job.name.endsWith(suffix));
  requireCondition(
    matches.length === 1 && matches[0].status === "completed" &&
      matches[0].conclusion === "success",
    `Missing unique successful job: ${suffix}`,
  );
}
/** @param {PackageRun} run @param {string} headSha */
export function validateSourceIdentity(run, headSha) {
  requireCondition(
    run.repository?.full_name === "Floorp-Projects/Floorp" &&
      run.path === ".github/workflows/package_and_publish_release.yml" &&
      run.head_sha === headSha && run.event === "workflow_dispatch",
    "Expected the exact canonical package run and source",
  );
}
/** @param {FinalSourceInput} input */
export function validateFinalPackageSource(
  {
    run,
    headSha,
    jobs,
    prePublication = false,
    unsigned = false,
    pendingDeployments = [],
    release = null,
    bundleArtifacts = [],
  },
) {
  validateSourceIdentity(run, headSha);
  for (const target of TARGETS) successfulJob(jobs, `Verify ${target} package`);
  if (!prePublication) {
    requireCondition(
      run.status === "completed" && run.conclusion === "success",
      "Expected a completed successful canonical package run",
    );
    return { phase: "completed" };
  }
  requireCondition(
    !unsigned && run.head_branch === "main" &&
      ["in_progress", "waiting"].includes(run.status) &&
      run.conclusion === null,
    "Pre-publication QA requires an unfinished signed production run on main",
  );
  successfulJob(
    jobs,
    "Validate provenance and assemble the flat release bundle",
  );
  successfulJob(jobs, "Validate immutable bundle and create draft release");
  // GitHub's pending-deployments response supplies the protected environment
  // and its required reviewers. It cannot be substituted by a caller's input.
  requireCondition(
    pendingDeployments.length === 1 &&
      pendingDeployments[0].environment?.name ===
        "Deploy-to-installer-release" &&
      Number.isSafeInteger(pendingDeployments[0].environment?.id) &&
      pendingDeployments[0].environment.id > 0 &&
      pendingDeployments[0].reviewers?.length > 0 &&
      pendingDeployments[0].reviewers.every((entry) =>
        ["User", "Team"].includes(entry.type) &&
        Number.isSafeInteger(entry.reviewer?.id) && entry.reviewer.id > 0
      ),
    "The exact run must await protected installer approval with required reviewers",
  );
  const publicationJobs = jobs.filter((job) =>
    [
      "Manual approval and public release gate",
      "Publish and verify stable updater metadata v2",
      "Build the exact public Floorp Portable release",
      "Publish the exact source-run DEB to PPA",
    ].some((name) => job.name.includes(name))
  );
  requireCondition(
    publicationJobs.every((job) =>
      job.conclusion === null &&
      ["queued", "waiting", "pending"].includes(job.status)
    ),
    "A publication or distribution job has already executed",
  );
  // A read-only token may not see a draft. Public releases are always visible;
  // the successful canonical draft job already checks its manifest binding.
  requireCondition(
    release === null ||
      (release.draft === true && release.prerelease === false &&
        release.target_commitish === headSha),
    "Cannot accept a public or unrelated draft release",
  );
  requireCondition(
    bundleArtifacts.length === 1 && !bundleArtifacts[0].expired &&
      bundleArtifacts[0].name === "floorp-release-bundle-v2" &&
      bundleArtifacts[0].workflow_run?.id === run.id &&
      bundleArtifacts[0].workflow_run?.head_sha === headSha &&
      /^sha256:[0-9a-f]{64}$/.test(bundleArtifacts[0].digest || ""),
    "Expected one immutable production bundle from the exact run",
  );
  return {
    phase: "awaiting-publication-approval",
    bundle: {
      id: bundleArtifacts[0].id,
      name: bundleArtifacts[0].name,
      digest: bundleArtifacts[0].digest,
    },
    publicationApprovalPerformed: false,
  };
}
