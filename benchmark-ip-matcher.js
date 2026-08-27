const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { performance } = require('perf_hooks');
const { createIPMatcher } = require('.');

const MAX_EVENT_LOOP_BLOCK_MS = 50;

function getFixturePaths() {
  return fs.readdirSync(path.join(__dirname, 'fixtures', 'ip-matcher'))
    .filter((file) => file.endsWith('.json'))
    .sort()
    .map((file) => path.join(__dirname, 'fixtures', 'ip-matcher', file));
}

function readList(fixturePath) {
  const networks = JSON.parse(fs.readFileSync(fixturePath, 'utf8'));
  if (!Array.isArray(networks) || !networks.every((network) => typeof network === 'string')) {
    throw new Error(`Expected ${fixturePath} to contain an array of IP networks`);
  }
  return networks;
}

async function main() {
  const lists = getFixturePaths().map(readList);
  const networkCount = lists.reduce((total, list) => total + list.length, 0);
  let maxEventLoopDelay = 0;
  let previousTick = performance.now();
  const interval = setInterval(() => {
    const now = performance.now();
    maxEventLoopDelay = Math.max(maxEventLoopDelay, now - previousTick - 10);
    previousTick = now;
  }, 10);

  const startedAt = performance.now();
  const matcherPromises = [];
  const createCallDurations = [];
  let matchers;
  let buildDuration;
  try {
    for (const list of lists) {
      const createStartedAt = performance.now();
      matcherPromises.push(createIPMatcher(list));
      createCallDurations.push(performance.now() - createStartedAt);
      await new Promise(setImmediate);
    }
    matchers = await Promise.all(matcherPromises);
    buildDuration = performance.now() - startedAt;
  } finally {
    clearInterval(interval);
  }

  assert.ok(
    lists[0] && lists[0].length > 0 && matchers[0].has(lists[0][0]),
    'Expected matcher to contain an input network'
  );
  for (const [index, createCallDuration] of createCallDurations.entries()) {
    assert.ok(
      createCallDuration < MAX_EVENT_LOOP_BLOCK_MS,
      `Matcher creation for list ${index + 1} blocked the event loop for ${createCallDuration.toFixed(2)}ms`
    );
  }
  const largestListNetworks = lists.reduce(
    (largest, list) => Math.max(largest, list.length),
    0
  );
  console.log(JSON.stringify({
    lists: lists.length,
    networks: networkCount,
    largestListNetworks,
    createIPMatcherCallsMS: createCallDurations.map((duration) => Number(duration.toFixed(2))),
    asyncBuildMS: Number(buildDuration.toFixed(2)),
    maxEventLoopDelayMS: Number(maxEventLoopDelay.toFixed(2)),
  }, null, 2));
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
