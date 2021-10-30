# vim: sw=2 ts=2 sts=2 expandtab
#
# Display a nice animation.

(defn I_START_init []
  # Copy over palette
  (swibnk 1)
  (def palette (peek 0x4000 (- 0x4040 0x4000)))
  (swibnk 0)
  (poke 0x4000 palette)

  # Put random characters everywhere
  (loop [y :range [0 height]]
    (loop [x :range [0 width]]
      (color (+ 1 (rand 14)))
      (c7put x y (string/from-bytes (+ 20 (rand 96)))))))

(var n 0)
(defn I_START_step []
  (++ n)
  (cond
    (< n 5)
      (loop [i :range [0x4040 0x52a0]]
        (poke i (if (= (rand (* n n)) 0) 1 0)))
    (= n 6)
      (do
        (color 0)
        (fill 0 0 width height " "))
    (> n 8)
      (swimd 1)))
