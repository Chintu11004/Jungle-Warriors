const id = "PT_EVENT_ACTOR_PERMANENT_DESPAWN";
const groups = ["Plugin Pak", "EVENT_GROUP_ACTOR"];
const name = "Permanently Despawn Actor";

const fields = [].concat([
  {
    key: "actorId",
    label: "Actor",
    type: "actor",
    defaultValue: "$self$",
  },
  {
    label:
      "Despawns actor until this scene is fully reloaded (deferred load/activate will not revive it).",
  },
]);

const compile = (input, helpers) => {
  const { actorSetById, appendRaw, _declareLocal, _addComment, _addNL } =
    helpers;

  const actorRef = _declareLocal("actor", 4);

  _addComment("Permanently despawn actor until scene reload");
  actorSetById(input.actorId);

  appendRaw(`

VM_ACTOR_PERM_DESPAWN ${actorRef}
  `);

  _addNL();
};

module.exports = {
  id,
  name,
  groups,
  fields,
  compile,
  waitUntilAfterInitFade: true,
};
