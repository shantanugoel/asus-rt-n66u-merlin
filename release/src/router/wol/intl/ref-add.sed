/^# Packages using this file: / {
  s/# Packages using this file://
  ta
  :a
  s/ wol / wol /
  tb
  s/ $/ wol /
  :b
  s/^/# Packages using this file:/
}
