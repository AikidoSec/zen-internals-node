const { test } = require('node:test');
const assert = require('node:assert');
const { AsyncLocalStorage } = require('node:async_hooks');
const { setCodeGenerationCallback } = require('.');

const store = new AsyncLocalStorage();

setCodeGenerationCallback((source) => {
  const ctx = store.getStore();
  if (ctx && ctx.blocked) {
    return `Blocked eval in request ${ctx.requestId}`;
  }
});

test('allows eval without async context', () => {
  assert.strictEqual(eval('1 + 1'), 2);
});

test('allows eval when context has blocked: false', () => {
  store.run({ requestId: 'req-123', blocked: false }, () => {
    assert.strictEqual(eval('2 + 2'), 4);
  });
});

test('blocks eval when context has blocked: true', () => {
  store.run({ requestId: 'req-456', blocked: true }, () => {
    assert.throws(
      () => eval('3 + 3'),
      {
        name: 'EvalError',
        message: 'Blocked eval in request req-456'
      }
    );
  });
});

test('blocks new Function when context has blocked: true', () => {
  store.run({ requestId: 'req-789', blocked: true }, () => {
    assert.throws(
      () => new Function('return 1'),
      {
        name: 'EvalError',
        message: 'Blocked eval in request req-789'
      }
    );
  });
});
