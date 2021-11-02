# vim: sw=2 ts=2 sts=2 expandtab

(def errorstr " error ")

(defn I_ERROR_init []
  # Reset font
  (swibnk 1)
  (def fonts (peek 0x4040 (- 0x52a0 0x4040)))
  (swibnk 0)
  (poke 0x4040 fonts)

  # Put random characters everywhere, excluding lowercase
  (loop [y :range [0 height]]
    (loop [x :range [0 width]]
      (color (+ 1 (rand 14)))
      (c7put x y (string/from-bytes (+ 32 (rand 56))))))
  (color 1)
  (let [x    (- (// width 2) (// (length errorstr) 2))
        y    (// height 2)
        spcs (string/repeat " " (length errorstr))
       ]
    (c7put x (- y 1) spcs)
    (c7put x (- y 0) errorstr)
    (c7put x (+ y 1) spcs)))

(defn I_ERROR_step []
  (delay 1)
  (def sparsity (* (+ (// (ticks) 7) 1) 7))
  (loop [i :range [(+ 0x4040 (* 1 49)) (+ 0x4040 (* 56 49))]]
    (poke i (if (= (rand sparsity) 0) 1 0))))
