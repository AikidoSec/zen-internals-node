const path = require('path');
const fs = require('fs');

// Load the correct prebuilt binary for this platform/arch/ABI
const platform = process.platform;
const arch = process.arch;
const abi = process.versions.modules;

const bindingPath = path.join(__dirname, 'prebuilds', `${platform}-${arch}`, `node-v${abi}.node`);

if (!fs.existsSync(bindingPath)) {
  throw new Error(
    `No prebuilt binary found for ${platform}-${arch} with ABI ${abi} (Node.js ${process.version}). ` +
    `Expected: ${bindingPath}`
  );
}

const binding = require(bindingPath);

module.exports = {
  setCodeGenerationCallback: binding.setCodeGenerationCallback
};
