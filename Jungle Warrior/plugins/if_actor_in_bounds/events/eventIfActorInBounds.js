/**
 * True when the actor's tile position lies inside the axis-aligned rectangle
 * defined by (x1,y1) and (x2,y2). Each corner is a value field (number, variable,
 * or expression). Either diagonal order works (axis range is union of the two
 * orderings).
 */

/** Coerce legacy plain numbers or ScriptValue from "value" fields. */
function asScriptValueCorner(raw, fallbackNum) {
  if (raw == null) {
    return { type: "number", value: fallbackNum };
  }
  if (typeof raw === "number" && Number.isFinite(raw)) {
    return { type: "number", value: raw };
  }
  if (typeof raw === "object" && raw !== null && "type" in raw) {
    return raw;
  }
  return { type: "number", value: fallbackNum };
}

function actorInBoundsScriptValue(input) {
  const actor = input.actorId;

  const svX1 = asScriptValueCorner(input.x1, 0);
  const svY1 = asScriptValueCorner(input.y1, 0);
  const svX2 = asScriptValueCorner(input.x2, 19);
  const svY2 = asScriptValueCorner(input.y2, 17);

  const xpos = { type: "property", target: actor, property: "xpos" };
  const ypos = { type: "property", target: actor, property: "ypos" };

  /** x in [min(x1,x2), max(x1,x2)] without min/max ops: (x1..x2) or (x2..x1). */
  const axisInRange = (pos, a, b) => ({
    type: "or",
    valueA: {
      type: "and",
      valueA: { type: "gte", valueA: pos, valueB: a },
      valueB: { type: "lte", valueA: pos, valueB: b },
    },
    valueB: {
      type: "and",
      valueA: { type: "gte", valueA: pos, valueB: b },
      valueB: { type: "lte", valueA: pos, valueB: a },
    },
  });

  return {
    type: "and",
    valueA: axisInRange(xpos, svX1, svX2),
    valueB: axisInRange(ypos, svY1, svY2),
  };
}

export const id = "EVENT_IF_ACTOR_IN_BOUNDS";
export const name = "If Actor In Bounds";
export const groups = ["Custom"];
export const isConditional = true;

export const fields = [
  {
    key: "actorId",
    label: "Actor",
    description: "Actor whose position is tested (tile xpos / ypos).",
    type: "actor",
    defaultValue: "$self$",
    width: "100%",
  },
  {
    key: "x1",
    label: "Corner 1 X (tiles)",
    description: "Number, variable, or expression (e.g. $00$ + $01$).",
    type: "value",
    min: 0,
    max: 255,
    width: "25%",
    unitsDefault: "tiles",
    defaultValue: {
      type: "number",
      value: 0,
    },
  },
  {
    key: "y1",
    label: "Corner 1 Y (tiles)",
    description: "Number, variable, or expression.",
    type: "value",
    min: 0,
    max: 255,
    width: "25%",
    unitsDefault: "tiles",
    defaultValue: {
      type: "number",
      value: 0,
    },
  },
  {
    key: "x2",
    label: "Corner 2 X (tiles)",
    description: "Number, variable, or expression.",
    type: "value",
    min: 0,
    max: 255,
    width: "25%",
    unitsDefault: "tiles",
    defaultValue: {
      type: "number",
      value: 19,
    },
  },
  {
    key: "y2",
    label: "Corner 2 Y (tiles)",
    description: "Number, variable, or expression.",
    type: "value",
    min: 0,
    max: 255,
    width: "25%",
    unitsDefault: "tiles",
    defaultValue: {
      type: "number",
      value: 17,
    },
  },
  {
    key: "true",
    label: "True",
    description: "Runs when the actor is inside the rectangle (edges included).",
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
    description: "Runs when the actor is outside the rectangle.",
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

  ifScriptValue(actorInBoundsScriptValue(input), truePath, falsePath);
};
