export const id = "EVENT_REGISTER_ROPE_ACTOR";
export const name = "Register Rope Actor";
export const groups = ["Custom"];

export const fields = [
  {
    key: "actor",
    label: "Actor",
    type: "actor",
    defaultValue: "$self$",
  },
];

export const compile = (input, helpers) => {
  const { _invoke, actorPushById } = helpers;

  // stack_frame[0] = actor index (from actorPushById)
  actorPushById(input.actor);

  // One-shot invoke: adds actor to rope_actor_indices for special activation/deactivation
  _invoke("rope_actor_register_invoke", 1, -1);
};
