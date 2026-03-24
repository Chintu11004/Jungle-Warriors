/**
 * Detection is compiled entirely in GBVM (same idea as built-in
 * eventIfActorDistanceFromActor / ifScriptValue): no scratch global, no VM_INVOKE.
 *
 * True when |watcher_x - target_x| <= range (tiles) and, if enabled, watcher_y == target_y.
 */

function playerDetectedScriptValue(input) {
  const watcher = input.actorWatcher;
  const target = input.actorTarget;
  const rangeTiles = input.rangeTiles ?? 6;
  const requireSameY = input.requireSameY !== false;

  const dx = {
    type: "sub",
    valueA: { type: "property", target: watcher, property: "xpos" },
    valueB: { type: "property", target: target, property: "xpos" },
  };

  const inHorizontalRange = {
    type: "lte",
    valueA: { type: "abs", value: dx },
    valueB: { type: "number", value: rangeTiles },
  };

  if (!requireSameY) {
    return inHorizontalRange;
  }

  return {
    type: "and",
    valueA: inHorizontalRange,
    valueB: {
      type: "eq",
      valueA: { type: "property", target: watcher, property: "ypos" },
      valueB: { type: "property", target: target, property: "ypos" },
    },
  };
}

export const id = "EVENT_IF_PLAYER_DETECTED";
export const name = "If Player Detected";
export const groups = ["Custom"];
export const isConditional = true;

export const fields = [
  {
    key: "actorWatcher",
    label: "Watcher actor",
    description: "Usually the enemy ($self$)",
    type: "actor",
    defaultValue: "$self$",
    width: "50%",
  },
  {
    key: "actorTarget",
    label: "Target actor",
    description: "Usually the player",
    type: "actor",
    defaultValue: "player",
    width: "50%",
  },
  {
    key: "rangeTiles",
    label: "Horizontal range (tiles)",
    type: "number",
    min: 0,
    max: 32,
    defaultValue: 6,
    width: "50%",
  },
  {
    key: "requireSameY",
    label: "Same tile row (Y)",
    description:
      "If true, both actors must be on the same tile row before distance is checked.",
    type: "checkbox",
    defaultValue: true,
    width: "50%",
  },
  {
    key: "true",
    label: "True",
    description: "Runs when the target is detected",
    type: "events",
  },
  {
    key: "__collapseElse",
    label: "Else",
    type: "collapsable",
    defaultValue: true,
    conditions: [
      {
        key: "__disableElse",
        ne: true,
      },
    ],
  },
  {
    key: "false",
    label: "False",
    description: "Runs when not detected",
    conditions: [
      {
        key: "__collapseElse",
        ne: true,
      },
      {
        key: "__disableElse",
        ne: true,
      },
    ],
    type: "events",
  },
];

export const compile = (input, helpers) => {
  const { ifScriptValue } = helpers;

  const truePath = input.true || [];
  const falsePath = input.__disableElse ? [] : input.false || [];

  ifScriptValue(
    playerDetectedScriptValue(input),
    truePath,
    falsePath,
  );
};
