// SPDX-License-Identifier: MPL-2.0
// Synthetic host-test fixtures, not native evidence schemas.
export interface AdmissionOptions {
  unsigned?: boolean;
  attempt?: string;
  prePublication?: boolean;
}

export interface RawArtifactFixture {
  id: number;
  size_in_bytes?: number;
  name: string;
  expired: boolean;
  digest?: string;
  workflow_run: { id: number; head_sha: string };
}

export interface AdmissionFixture {
  run: {
    id: number;
    repository: { full_name: string };
    path: string;
    head_sha: string;
    head_branch: string;
    event: string;
    run_attempt: number;
    status: string;
    conclusion: string | null;
  };
  jobs: Array<{ name: string; status: string; conclusion: string | null }>;
  artifacts: RawArtifactFixture[];
  pending: Array<{
    environment: { id: number; name: string };
    reviewers: Array<{ type: string; reviewer: { id: number } }>;
  }>;
  release: { draft: boolean; prerelease: boolean; target_commitish: string };
}
