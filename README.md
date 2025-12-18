# zen-internals-node

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
