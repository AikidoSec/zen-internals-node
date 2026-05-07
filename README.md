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
