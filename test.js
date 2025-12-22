const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');
const { setCodeGenerationCallback } = require('.');

const store = new AsyncLocalStorage();

setCodeGenerationCallback((source) => {
  const ctx = store.getStore();
  if (ctx && ctx.blocked) {
    return `Blocked eval in request ${ctx.requestId}`;
  }
});

// Test: allows eval without async context
assert.strictEqual(eval('1 + 1'), 2);

// Test: allows eval when context has blocked: false
store.run({ requestId: 'req-123', blocked: false }, () => {
  assert.strictEqual(eval('2 + 2'), 4);
});

// Test: blocks eval when context has blocked: true
store.run({ requestId: 'req-456', blocked: true }, () => {
  assert.throws(
    () => eval('3 + 3'),
    {
      name: 'EvalError',
      message: 'Blocked eval in request req-456'
    }
  );
});

// Test: blocks new Function when context has blocked: true
store.run({ requestId: 'req-789', blocked: true }, () => {
  assert.throws(
    () => new Function('return 1'),
    {
      name: 'EvalError',
      message: 'Blocked eval in request req-789'
    }
  );
});

// Test: blocks Function constructor accessed via prototype chain
store.run({ requestId: 'req-proto', blocked: true }, () => {
  assert.throws(
    () => ({}).constructor.constructor('return 1')(),
    {
      name: 'EvalError',
      message: 'Blocked eval in request req-proto'
    }
  );
});

// Test override callback to allow all
setCodeGenerationCallback((source) => {
  return undefined; // Allow all
});
// Should no longer block
store.run({ requestId: 'req-allow', blocked: true }, () => {
   assert.strictEqual(eval('4 + 4'), 8);
});

console.log('All tests passed');

// TODO test vm module with the dynamic code generation options
// TODO test with the --disallow-code-generation-from-strings flag
