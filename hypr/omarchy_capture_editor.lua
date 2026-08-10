-- Omarchy Capture Editor integration. Loaded after Omarchy's default bindings.
hl.unbind("PRINT")
o.bind("PRINT", "Screenshot", "omarchy-capture-editor")

hl.layer_rule({
  match = { namespace = "^omarchy-capture-editor$" },
  no_anim = true,
  animation = "none",
})
