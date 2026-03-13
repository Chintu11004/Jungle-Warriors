export const id = "EVENT_DEFERRED_ACTOR_UNLOAD";
export const name = "Deferred Actor Unload";
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

  // One-shot invoke: handler returns TRUE immediately after processing
  _invoke("deferred_actor_unload", 1, -1);
};
