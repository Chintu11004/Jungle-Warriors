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
  {
    key: "rope_swing_speed_factor",
    label: "Swing Speed factor (1/n)",
    type: "number",
    defaultValue: 1,
    min: 1,
    max: 5,
  },
  {
    key: "rope_actor",
    label: "Rope Actor",
    type: "actor",
    defaultValue: "$self$",
  },
];

export const compile = (input, helpers) => {
  const { _invoke, _stackPushConst, actorPushById } = helpers;

  // Push in forward order so stack_frame[0]=anchor_x, [1]=anchor_y, etc.
  // The VM convention: PARAMS = -(N args), stack_frame[0] = first pushed (deepest).
  // stack_frame layout inside rope_swing_update:
  //   [0] anchor_x       (first pushed, deepest)
  //   [1] anchor_y
  //   [2] length
  //   [3] max_angle
  //   [4] speed_factor
  //   [5] actor_idx     (resolved from actor UUID by actorPushById)
  _stackPushConst(input.rope_anchor_x || 0);              // stack_frame[0]
  _stackPushConst(input.rope_anchor_y || 0);              // stack_frame[1]
  _stackPushConst(input.rope_length || 5);                // stack_frame[2]
  _stackPushConst(input.rope_max_angle || 45);            // stack_frame[3]
  _stackPushConst(input.rope_swing_speed_factor || 1);    // stack_frame[4]
  actorPushById(input.rope_actor);                       // stack_frame[5] - resolves actor UUID to index

  // VM_INVOKE calls rope_swing_update every frame until it returns TRUE.
  // popCount=6 tells the VM to pop the 6 stack slots when done.
  // PARAMS=-6 so stack_frame = stack_ptr - 6, pointing at the first pushed arg.
  _invoke("rope_swing_update", 6, -6);
};
