import assert from 'node:assert/strict';
import test from 'node:test';
import { validateSourceIdentity } from '../final-package-qa-source.mjs';

test('successful development workflow cannot enter canonical final package intake', () => {
  const run = {repository:{full_name:'Floorp-Projects/Floorp'},path:'.github/workflows/colocated_runner_test.yml',head_sha:'a'.repeat(40),event:'workflow_dispatch',status:'completed',conclusion:'success'};
  assert.throws(() => validateSourceIdentity(run,run.head_sha),/canonical package run/);
});
