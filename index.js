const binding = require('./build/Release/codegen_hook.node');

module.exports = {
  setCodeGenerationCallback: binding.setCodeGenerationCallback
};
