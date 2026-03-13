export const id = "EVENT_DELOAD_PREV_LEVEL";
export const name = "Deload Prev Level";
export const groups = ["Custom"];

export const compile = (input, helpers) => {
  const { _invoke } = helpers;

  // One-shot invoke: unloads all deferred actors, resets slots, reclaims VRAM.
  // Call on level transition—no need to manually unload each actor first.
  _invoke("deload_prev_level_invoke", 0, 0);
};
