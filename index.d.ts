export type CodeGenerationCallback = (source: string) => string | void;

export function setCodeGenerationCallback(callback: CodeGenerationCallback): void;
