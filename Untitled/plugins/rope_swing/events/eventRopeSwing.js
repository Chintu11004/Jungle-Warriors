export const id = "EVENT_ROPE_SWING";
export const name = "Rope Swing";
export const groups = ["Custom"];

export const fields = [
  {
    key: "rope_anchor_x",
    label: "Rope Anchor X (tiles)",
    type: "number",
    defaultValue: 0,
  },
  {
    key: "rope_anchor_y",
    label: "Rope Anchor Y (tiles)",
    type: "number",
    defaultValue: 0,
  },
  {
    key: "rope_length",
    label: "Rope Length (tiles)",
    type: "number",
    defaultValue: 5,
    min: 1,
    max: 20,
  },
  {
    key: "rope_max_angle",
    label: "Max Swing Angle (degrees)",
    type: "number",
    defaultValue: 45,
    min: 30,
    max: 90,
  },
];

export const compile = (input, helpers) => {
  const { _callNative, _stackPushConst, _stackPop } = helpers;
  
  // Push parameters to stack in reverse order (right to left)
  // They will be popped as: anchor_x, anchor_y, length, max_angle_degrees
  _stackPushConst(input.rope_max_angle || 45);  // degrees (30-90)
  _stackPushConst(input.rope_length || 5);
  _stackPushConst(input.rope_anchor_y || 0);
  _stackPushConst(input.rope_anchor_x || 0);
  
  // Call rope_swing_enter which will convert degrees to sine array index
  _callNative("rope_swing_enter");

  // _callNative does not pop its arguments — restore stack balance
  _stackPop(4);
};
