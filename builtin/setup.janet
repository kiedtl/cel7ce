# vim: sw=2 ts=2 sts=2 expandtab
#
# Initialize palette, fonts, color, etc.
# (Palette was probably already initialized in start.janet, but
# we're doing it here again anyway...)

(defn I_SETUP_init []
  (swibnk 1)
  (def data (peek 0x4000 (- 0x52a0 0x4000)))
  (swibnk 0)
  (poke 0x4000 data)

  # Set color to default.
  (color 1)

  # Switch to normal (if no error) or error mode.
  (swimd (if (lderr) 3 2)))
