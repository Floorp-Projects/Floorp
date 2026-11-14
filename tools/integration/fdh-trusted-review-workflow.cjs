// SPDX-License-Identifier: MPL-2.0
// Independent local-only workflow execution review. Run from repository root.
// Uses mocked GitHub APIs, real Deno for receipt validation; no network mutations.
const assert = require('node:assert/strict');
const { readFileSync } = require('node:fs');
const { spawn: realSpawn } = require('node:child_process');
const { EventEmitter } = require('node:events');
const yaml = readFileSync('.github/workflows/fdh-trusted-review-gate.yml', 'utf8');
const lines = yaml.split(/\r?\n/);
const start = lines.findIndex(line => line.trim() === 'script: |');
assert.ok(start >= 0);
const source = lines.slice(start + 1).map(line => line.slice(12)).join('\n');
assert.match(yaml, /name: trusted-receipt-publication/);
const AsyncFunction = Object.getPrototypeOf(async function () {}).constructor;
const head = 'a'.repeat(40), other = 'b'.repeat(40);
process.env.GITHUB_REPOSITORY = 'fixture/repo';
process.env.PR_NUMBER = '42';
process.env.GITHUB_TOKEN = 'FAKE_TEST_SENTINEL';
process.env.GH_TOKEN = 'FAKE_TEST_SENTINEL';
const receipt = (who, changes = {}) => ({
  body: '<!-- fdh-review-receipt:v1 ' + JSON.stringify({
    schemaVersion: 1, reviewer: who, model: 'fixture', commit: head,
    verdict: 'approve', blocking: false, scope: 'fixture', tier: 'full_read', ...changes,
  }) + ' -->',
  user: { login: 'trusted' }, author_association: 'MEMBER',
});
const approvals = [receipt('one'), receipt('two')];
async function run(label, comments, expected, options = {}) {
  process.env.EVENT_HEAD_SHA = options.eventHead ?? '';
  let reads = 0, created = 0, updated, spawns = 0;
  const operations = [];
  const failures = [];
  const github = {
    rest: {
      pulls: { get: async () => ({ data: { number: 42, head: {
        sha: ++reads === 2 && options.headRace ? other : head,
      } } }) },
      checks: {
        create: async input => {
          assert.equal(input.head_sha, head);
          assert.equal(input.name, 'fdh-trusted-receipts');
          assert.equal(input.status, 'in_progress');
          created++;
          return { data: { id: 123 } };
        },
        update: async input => {
          await Promise.resolve();
          updated = input;
          operations.push('custom-check-completed');
        },
      },
      issues: { listComments() {} },
    },
    paginate: async () => {
      if (options.apiFailure) throw new Error('fixture comments API failure');
      return comments;
    },
  };
  const core = { setFailed(message) {
    assert.equal(updated?.conclusion, 'failure', 'must publish failure before core.setFailed');
    failures.push(message);
    operations.push('workflow-failed');
  } };
  function spawn(file, args, childOptions) {
    spawns++;
    assert.equal(file, 'deno');
    assert.deepEqual(args, ['run', '--no-config', '--no-lock', '--quiet', 'tools/src/fdh_trusted_review_runner.ts']);
    assert.equal(childOptions.shell, false);
    assert.equal(childOptions.env.GITHUB_TOKEN, undefined);
    assert.equal(childOptions.env.GH_TOKEN, undefined);
    const allowed = ['DENO_NO_UPDATE_CHECK', 'PATH', 'SystemRoot', 'WINDIR'];
    assert.ok(Object.keys(childOptions.env).every(key => allowed.includes(key)));
    if (!options.childFailure && !options.invalidOutput) return realSpawn(file, args, childOptions);
    const child = new EventEmitter();
    child.stdout = new EventEmitter();
    child.stderr = new EventEmitter();
    child.stdin = { end() { queueMicrotask(() => {
      if (options.invalidOutput) child.stdout.emit('data', Buffer.from('{invalid JSON'));
      child.emit('close', options.invalidOutput ? 0 : 2);
    }); } };
    return child;
  }
  let thrown;
  try {
    await new AsyncFunction('require', 'github', 'context', 'core', source)(
      name => name === 'node:child_process' ? { spawn } : require(name),
      github, { repo: { owner: 'fixture', repo: 'repo' } }, core,
    );
  } catch (error) { thrown = error; }
  if (options.stale) {
    assert.ok(thrown);
    assert.equal(created, 0);
    assert.equal(spawns, 0);
    assert.equal(updated, undefined);
    assert.equal(failures.length, 0); // thrown exception fails Actions directly
  } else {
    assert.equal(thrown, undefined);
    assert.equal(created, 1);
    assert.equal(updated.check_run_id, 123);
    assert.equal(updated.status, 'completed');
    assert.equal(updated.conclusion, expected);
    assert.equal(failures.length, expected === 'failure' ? 1 : 0);
    assert.deepEqual(operations, expected === 'failure'
      ? ['custom-check-completed', 'workflow-failed'] : ['custom-check-completed']);
  }
  console.log('PASS', label, JSON.stringify({ conclusion: updated?.conclusion,
    workflowFailed: failures.length === 1 || Boolean(thrown), spawns }));
}
(async () => {
  await run('real Deno approvals', approvals, 'success');
  await run('edited marker removed', [approvals[0], { ...approvals[1], body: 'withdrawn' }], 'failure');
  await run('deleted receipt', [approvals[0]], 'failure');
  await run('blocking receipt', [approvals[0], receipt('two', { blocking: true })], 'failure');
  await run('head changes during validation', approvals, 'failure', { headRace: true });
  await run('comments API fails', [], 'failure', { apiFailure: true });
  await run('runner nonzero exit', [], 'failure', { childFailure: true });
  await run('malformed runner stdout', [], 'failure', { invalidOutput: true });
  await run('stale triggering head', approvals, undefined, { eventHead: other, stale: true });
  console.log('PASS all 9 workflow scenarios; custom rejection propagates AFTER publication.');
})().catch(error => { console.error(error); process.exitCode = 1; });
