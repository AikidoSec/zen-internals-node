const assert = require('assert');
const { createIPMatcher } = require('.');

async function test(name, run) {
  await run();
  console.log(`✓ ${name}`);
}

async function main() {
  await test('check with single IPv4s', async () => {
    const input = [
      '192.168.0.0/32',
      '192.168.0.3/32',
      '192.168.0.24/32',
      '192.168.0.52/32',
      '192.168.0.123/32',
      '192.168.0.124/32',
      '192.168.0.125/32',
      '192.168.0.170/32',
      '192.168.0.171/32',
      '192.168.0.222/32',
      '192.168.0.234/32',
      '192.168.0.255/32',
    ];
    const matcher = await createIPMatcher(input);
    assert.strictEqual(matcher.has('192.168.0.254'), false);
    assert.strictEqual(matcher.has('192.168.0.1'), false);
    assert.strictEqual(matcher.has('192.168.0.255'), true);
    assert.strictEqual(matcher.has('192.168.0.24'), true);
  });

  await test('it works with ranges', async () => {
    const input = [
      '192.168.0.0/24',
      '192.168.0.3/32',
      '192.168.0.24/32',
      '192.168.0.52/32',
      '192.168.0.123/32',
      '192.168.0.124/32',
      '192.168.0.125/32',
      '192.168.0.170/32',
      '192.168.0.171/32',
      '192.168.0.222/32',
      '192.168.0.234/32',
      '192.168.0.255/32',
    ];
    const matcher = await createIPMatcher(input);
    assert.strictEqual(matcher.has('192.168.0.254'), true);
    assert.strictEqual(matcher.has('10.0.0.1'), false);
    assert.strictEqual(matcher.has('192.168.0.234'), true);
  });

  await test('it works with invalid ranges', async () => {
    const input = [
      '192.168.0.0/24',
      '192.168.0.3/32',
      '192.168.0.24/32',
      '192.168.0.52/32',
      'foobar',
      '0.a.0.0/32',
      '123.123.123.123/1999',
      '',
      ',,,',
      '192.168.0.124/32',
      '192.168.0.125/32',
      '192.168.0.170/32',
      '192.168.0.171/32',
      '192.168.0.222/32',
      '192.168.0.234/32',
      '192.168.0.255',
    ];
    const matcher = await createIPMatcher(input);
    assert.strictEqual(matcher.has('192.168.0.254'), true);
    assert.strictEqual(matcher.has('foobar'), false);
    assert.strictEqual(matcher.has('192.168.0.222'), true);
    assert.strictEqual(matcher.has('192.168.0.1'), true);
    assert.strictEqual(matcher.has('10.0.0.1'), false);
    assert.strictEqual(matcher.has('192.168.0.255'), true);
    assert.strictEqual(matcher.has(''), false);
    assert.strictEqual(matcher.has('1'), false);
    assert.strictEqual(matcher.has('192.168.0.1/32'), true);
  });

  await test('it works with empty ranges', async () => {
    const matcher = await createIPMatcher([]);
    assert.strictEqual(matcher.has('192.168.2.1'), false);
    assert.strictEqual(matcher.has('foobar'), false);
  });

  await test('it propagates errors while reading the network array', async () => {
    const input = [];
    Object.defineProperty(input, 0, {
      get() {
        throw new Error('Failed to read network');
      },
    });
    input.length = 1;

    assert.throws(() => createIPMatcher(input), /Failed to read network/);
  });

  await test('it rejects sparse and oversized network arrays', async () => {
    assert.throws(() => createIPMatcher(new Array(1)), /dense array/);
    assert.throws(() => createIPMatcher(new Array(1_000_001)), /at most 1000000 networks/);
  });

  await test('it works with IPv6 ranges', async () => {
    const input = [
      '2002:db8::/32',
      '2001:db8::1/128',
      '2001:db8::2/128',
      '2001:db8::3/128',
      '2001:db8::4/128',
      '2001:db8::5/128',
      '2001:db8::6/128',
      '2001:db8::7/128',
      '2001:db8::8/128',
      '2001:db8::9/128',
      '2001:db8::a/128',
      '2001:db8::b/128',
      '2001:db8::c/128',
      '2001:db8::d/128',
      '2001:db8::e/128',
      '[2001:db8::f]',
      '2001:db9::abc',
    ];
    const matcher = await createIPMatcher(input);
    assert.strictEqual(matcher.has('2001:db8::1'), true);
    assert.strictEqual(matcher.has('2001:db8::0'), false);
    assert.strictEqual(matcher.has('2001:db8::f'), true);
    assert.strictEqual(matcher.has('[2001:db8::f]'), true);
    assert.strictEqual(matcher.has('2001:db8::10'), false);
    assert.strictEqual(matcher.has('2002:db8::1'), true);
    assert.strictEqual(matcher.has('2002:db8::2f:2'), true);
    assert.strictEqual(matcher.has('2001:db9::abc'), true);
  });

  await test('mix IPv4 and IPv6', async () => {
    const matcher = await createIPMatcher(['2002:db8::/32', '10.0.0.0/8']);

    assert.strictEqual(matcher.has('2001:db8::1'), false);
    assert.strictEqual(matcher.has('2001:db8::0'), false);
    assert.strictEqual(matcher.has('2002:db8::1'), true);
    assert.strictEqual(matcher.has('10.0.0.1'), true);
    assert.strictEqual(matcher.has('10.0.0.255'), true);
    assert.strictEqual(matcher.has('192.168.1.1'), false);
  });

  await test('strange IPs', async () => {
    const input = ['::ffff:0.0.0.0', '::ffff:0:0:0:0', '::ffff:127.0.0.1'];
    const matcher = await createIPMatcher(input);

    assert.strictEqual(matcher.has('::ffff:0.0.0.0'), true);
    assert.strictEqual(matcher.has('::ffff:127.0.0.1'), true);
    assert.strictEqual(matcher.has('::ffff:123'), false);
    assert.strictEqual(matcher.has('2001:db8::1'), false);
    assert.strictEqual(matcher.has('[::ffff:0.0.0.0]'), true);
    assert.strictEqual(matcher.has('::ffff:0:0:0:0'), true);
  });

  await test('different CIDR ranges', async () => {
    const tests = [
      ['123.2.0.2/0', '1.1.1.1', true],
      ['123.2.0.2/1', '1.1.1.1', true],
      ['123.2.0.2/2', '1.1.1.1', false],
      ['123.2.0.2/3', '123.3.0.1', true],
      ['123.2.0.2/4', '123.3.0.1', true],
      ['123.2.0.2/5', '123.3.0.1', true],
      ['123.2.0.2/6', '123.3.0.1', true],
      ['123.2.0.2/7', '123.3.0.1', true],
      ['123.2.0.2/8', '123.3.0.1', true],
      ['123.2.0.2/9', '123.3.0.1', true],
      ['123.2.0.2/10', '123.3.0.1', true],
      ['123.2.0.2/11', '123.3.0.1', true],
      ['123.2.0.2/12', '123.3.0.1', true],
      ['123.2.0.2/13', '123.3.0.1', true],
      ['123.2.0.2/14', '123.3.0.1', true],
      ['123.2.0.2/15', '123.3.0.1', true],
      ['123.2.0.2/16', '123.3.0.1', false],
      ['123.2.0.2/17', '123.2.0.1', true],
      ['123.2.0.2/18', '123.2.0.1', true],
      ['123.2.0.2/19', '123.2.0.1', true],
      ['123.2.0.2/20', '123.2.0.1', true],
      ['123.2.0.2/21', '123.2.0.1', true],
      ['123.2.0.2/22', '123.2.0.1', true],
      ['123.2.0.2/23', '123.2.0.1', true],
      ['123.2.0.2/24', '123.2.0.1', true],
      ['123.2.0.2/25', '123.2.0.1', true],
      ['123.2.0.2/26', '123.2.0.1', true],
      ['123.2.0.2/27', '123.2.0.1', true],
      ['123.2.0.2/29', '123.2.0.1', true],
      ['123.2.0.2/30', '123.2.0.1', true],
      ['123.2.0.2/31', '123.2.0.1', false],
      ['123.2.0.2/32', '123.2.0.2', true],
    ];

    for (const [network, address, expected] of tests) {
      const matcher = await createIPMatcher([network]);
      assert.strictEqual(matcher.has(address), expected);
    }
  });

  await test('allow all IPs', async () => {
    const matcher = await createIPMatcher(['0.0.0.0/0', '::/0']);
    assert.strictEqual(matcher.has('1.2.3.4'), true);
    assert.strictEqual(matcher.has('::1'), true);
    assert.strictEqual(matcher.has('::ffff:1234'), true);
    assert.strictEqual(matcher.has('1.1.1.1'), true);
    assert.strictEqual(matcher.has('2002:db8::1'), true);
    assert.strictEqual(matcher.has('10.0.0.1'), true);
    assert.strictEqual(matcher.has('10.0.0.255'), true);
    assert.strictEqual(matcher.has('192.168.1.1'), true);
  });

  await test('adjacent /4 ranges at end of address space', async () => {
    // 224.0.0.0/4 and 240.0.0.0/4 merge to 224.0.0.0/3.
    const matcher = await createIPMatcher(['224.0.0.0/4', '240.0.0.0/4']);

    assert.strictEqual(matcher.has('224.0.0.1'), true);
    assert.strictEqual(matcher.has('240.0.0.1'), true);
    assert.strictEqual(matcher.has('255.255.255.255'), true);
    assert.strictEqual(matcher.has('223.255.255.255'), false);
  });
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
