# zen-internals-node

This repository contains a native Node.js addon used by [Zen for Node.js](https://github.com/AikidoSec/firewall-node).

```js
import { setCodeGenerationCallback } from 'zen-internals-node';

setCodeGenerationCallback((code) => {
  console.log('Generated code:', code);
});
```

You can also block code generation by returning a string from the callback:

```js
import { setCodeGenerationCallback } from 'zen-internals-node';

setCodeGenerationCallback((code) => {
  return 'Code generation blocked because ...';
});
```

## IP matcher

Matcher construction runs asynchronously. Once constructed, lookups are synchronous.

```js
import { createIPMatcher } from 'zen-internals-node';

const matcher = await createIPMatcher([
  '10.0.0.0/8',
  '2001:db8::/32',
]);

matcher.has('10.0.0.1'); // true
matcher.has('192.168.0.1'); // false
```

Invalid networks are ignored. `createIPMatcher` throws when its argument is not an array of strings.

IPv4-mapped IPv6 addresses match IPv4 networks: `matcher.has('::ffff:10.0.0.1')` is `true`. Lookups longer than 128 bytes never match.

The matcher is the Rust `zen_ip_matcher` crate from [zen-internals](https://github.com/AikidoSec/zen-internals), built by `npm run build:rust` (needs a Rust toolchain) before node-gyp links it into the addon.
