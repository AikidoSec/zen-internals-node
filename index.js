const path = require('path');
const fs = require('fs');

// Load the correct prebuilt binary for this platform/arch/node version
const platform = process.platform;
const arch = process.arch;
const nodeMajor = parseInt(process.versions.node.split('.')[0], 10);

// Supported: 16, 18-25 (17 excluded due to V8 callback issues)
const supportedVersions = [16, 18, 19, 20, 21, 22, 23, 24, 25];
if (!supportedVersions.includes(nodeMajor)) {
  throw new Error(
    `zen-internals-node: Unsupported Node.js version ${process.version}. ` +
    `Supported versions: ${supportedVersions.join(', ')}.`
  );
}

const bindingPath = path.join(__dirname, 'prebuilds', `${platform}-${arch}`, `zen-internals-node.node${nodeMajor}.node`);

if (!fs.existsSync(bindingPath)) {
  throw new Error(
    `No prebuilt binary found for ${platform}-${arch} (Node.js ${process.version}). ` +
    `Expected: ${bindingPath}`
  );
}

const binding = require(bindingPath);

module.exports = {
  setCodeGenerationCallback: binding.setCodeGenerationCallback
};
