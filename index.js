// node-gyp-build automatically loads prebuilt binaries from prebuilds/
// or falls back to build/Release if no prebuild is available
const binding = require('node-gyp-build')(__dirname);

module.exports = {
  setCodeGenerationCallback: binding.setCodeGenerationCallback
};
