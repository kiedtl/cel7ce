# vim: sw=2 ts=2 sts=2 expandtab
#
# Initialize palette and fonts.
# (Palette was probably already initialized in start.janet, but
# we're doing it here again anyway...)

(defn I_SETUP_init []
  (swibnk 1)
  (def data (peek 0x4000 (- 0x52a0 0x4000)))
  (swibnk 0)
  (poke 0x4000 data)
  (swimd 2))
