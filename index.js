const path = require('path');
const fs = require('fs');

// Load the correct prebuilt binary for this platform/arch/node version
const platform = process.platform;
const arch = process.arch;
const nodeMajor = process.versions.node.split('.')[0];

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
