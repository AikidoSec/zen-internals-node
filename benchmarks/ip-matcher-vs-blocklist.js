const assert = require("assert");
const net = require("net");
const { performance } = require("perf_hooks");
const { createIPMatcher } = require("..");
const { getFixturePaths, readList } = require("./fixtures");

// Added in Node.js v26.8.0
// See https://nodejs.org/api/net.html#class-netblocklist
const HAS_BATCH_BLOCKLIST_API =
  typeof net.BlockList.prototype.addCIDRs === "function";

const LOOKUP_ITERATIONS = 200_000;

function addressFamily(address) {
  return address.includes(":") ? "ipv6" : "ipv4";
}

function categorizeNetworks(networks) {
  const cidrs = [];
  const addresses = { ipv4: [], ipv6: [] };
  for (const network of networks) {
    if (network.includes("/")) {
      cidrs.push(network);
    } else {
      addresses[addressFamily(network)].push(network);
    }
  }
  return { cidrs, addresses };
}

function buildBlockListWithBatchAPI(blockList, { cidrs, addresses }) {
  if (cidrs.length > 0) blockList.addCIDRs(cidrs);
  if (addresses.ipv4.length > 0) blockList.addAddresses(addresses.ipv4, "ipv4");
  if (addresses.ipv6.length > 0) blockList.addAddresses(addresses.ipv6, "ipv6");
}

function buildBlockListOneByOne(blockList, { cidrs, addresses }) {
  for (const cidr of cidrs) {
    const separatorIndex = cidr.lastIndexOf("/");
    const network = cidr.slice(0, separatorIndex);
    const prefix = Number(cidr.slice(separatorIndex + 1));
    blockList.addSubnet(network, prefix, addressFamily(network));
  }
  for (const address of addresses.ipv4) blockList.addAddress(address, "ipv4");
  for (const address of addresses.ipv6) blockList.addAddress(address, "ipv6");
}

function buildBlockList(networks) {
  const blockList = new net.BlockList();
  const categorized = categorizeNetworks(networks);
  if (HAS_BATCH_BLOCKLIST_API) {
    buildBlockListWithBatchAPI(blockList, categorized);
  } else {
    buildBlockListOneByOne(blockList, categorized);
  }
  return blockList;
}

// A handful of addresses drawn from the list itself (guaranteed matches) plus
// random addresses (almost certainly non-matches), reused across many
// lookups so we measure steady-state check() / has() cost.
function buildQuerySamples(networks) {
  const sampleSize = Math.min(2000, networks.length);
  const step = Math.max(1, Math.floor(networks.length / sampleSize));
  const positive = [];
  for (let i = 0; i < networks.length; i += step) {
    const network = networks[i];
    positive.push(
      network.includes("/") ? network.slice(0, network.indexOf("/")) : network,
    );
  }

  const negative = [];
  for (let i = 0; i < sampleSize; i++) {
    negative.push(
      i % 2 === 0
        ? `${203}.${i % 256}.${(i * 7) % 256}.${(i * 13) % 256}`
        : `2001:dead:beef:${(i % 65536).toString(16)}::${(i * 3) % 65536}`,
    );
  }

  return positive.concat(negative);
}

function benchmarkLookups(fn, samples, iterations) {
  const startedAt = performance.now();
  for (let i = 0; i < iterations; i++) {
    fn(samples[i % samples.length]);
  }
  return performance.now() - startedAt;
}

async function withEventLoopDelayProbe(fn) {
  let maxDelay = 0;
  let previousTick = performance.now();
  const interval = setInterval(() => {
    const now = performance.now();
    maxDelay = Math.max(maxDelay, now - previousTick - 10);
    previousTick = now;
  }, 10);

  const result = await fn();
  await new Promise((resolve) => setImmediate(resolve));
  clearInterval(interval);
  return { ...result, maxEventLoopDelayMS: Number(maxDelay.toFixed(2)) };
}

async function buildIPMatchers(lists) {
  return withEventLoopDelayProbe(async () => {
    const startedAt = performance.now();
    const matcherPromises = [];
    const createCallsMS = [];
    for (const list of lists) {
      const callStartedAt = performance.now();
      matcherPromises.push(createIPMatcher(list));
      createCallsMS.push(
        Number((performance.now() - callStartedAt).toFixed(2)),
      );
      await new Promise(setImmediate);
    }
    const matchers = await Promise.all(matcherPromises);
    const asyncBuildMS = Number((performance.now() - startedAt).toFixed(2));
    return { matchers, createCallsMS, asyncBuildMS };
  });
}

async function buildBlockLists(lists) {
  return withEventLoopDelayProbe(async () => {
    const startedAt = performance.now();
    const blockLists = lists.map(buildBlockList);
    const buildMS = Number((performance.now() - startedAt).toFixed(2));
    return { blockLists, buildMS };
  });
}

async function main() {
  const lists = getFixturePaths().map(readList);
  const networkCount = lists.reduce((total, list) => total + list.length, 0);
  const largestListNetworks = lists.reduce(
    (largest, list) => Math.max(largest, list.length),
    0,
  );

  const ipMatcherBuild = await buildIPMatchers(lists);
  const blockListBuild = await buildBlockLists(lists);
  const samplesByList = lists.map(buildQuerySamples);

  let ipMatcherLookupMS = 0;
  let blockListLookupMS = 0;
  lists.forEach((networks, index) => {
    const matcher = ipMatcherBuild.matchers[index];
    const blockList = blockListBuild.blockLists[index];
    const samples = samplesByList[index];

    // Sanity-check both structures agree before trusting the lookup numbers.
    for (const address of samples) {
      assert.strictEqual(
        matcher.has(address),
        blockList.check(address, addressFamily(address)),
        `list ${index + 1}: ip-matcher and net.BlockList disagreed on ${address}`,
      );
    }

    ipMatcherLookupMS += benchmarkLookups(
      (address) => matcher.has(address),
      samples,
      LOOKUP_ITERATIONS,
    );
    blockListLookupMS += benchmarkLookups(
      (address) => blockList.check(address, addressFamily(address)),
      samples,
      LOOKUP_ITERATIONS,
    );
  });
  const totalLookups = LOOKUP_ITERATIONS * lists.length;

  console.log(
    JSON.stringify(
      {
        lists: lists.length,
        networks: networkCount,
        largestListNetworks,
        ipMatcher: {
          createCallsMS: ipMatcherBuild.createCallsMS,
          asyncBuildMS: ipMatcherBuild.asyncBuildMS,
          maxEventLoopDelayMS: ipMatcherBuild.maxEventLoopDelayMS,
          lookupOpsPerSecond: Math.round(
            (totalLookups / ipMatcherLookupMS) * 1000,
          ),
        },
        blockList: {
          buildMS: blockListBuild.buildMS,
          maxEventLoopDelayMS: blockListBuild.maxEventLoopDelayMS,
          lookupOpsPerSecond: Math.round(
            (totalLookups / blockListLookupMS) * 1000,
          ),
        },
      },
      null,
      2,
    ),
  );
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
