# vim: sw=2 ts=2 sts=2 expandtab

(def height 40)
(def width 40)
(def scale 2)

(def chs ["|" "\\" "-" " "])
(def clr [0x1  0xD 0xF 0x0])
(def bkg [0xE  0xF 0x0 0x0])

(def sprites [
  [ "|"
    0 0 0 1 0 0 0
    0 0 0 0 0 0 0
    0 0 0 1 0 0 0
    0 0 0 0 0 0 0
    0 0 0 1 0 0 0
    0 0 0 1 0 0 0
    0 0 0 1 0 0 0
  ]
  [ "\\"
    1 0 0 0 0 0 0
    0 0 0 0 0 0 0
    0 0 1 0 0 0 0
    0 0 0 0 0 0 0
    0 0 0 0 1 0 0
    0 0 0 0 0 1 0
    0 0 0 0 0 0 1
  ]
  [ "-"
    0 0 0 0 0 0 0
    0 0 0 0 0 0 0
    0 0 0 0 0 0 0
    1 0 1 0 1 1 1
    0 0 0 0 0 0 0
    0 0 0 0 0 0 0
    0 0 0 0 0 0 0
  ]
])

(defn load-sprites [data]
    (loop [sprite :in data]
        (def char (- ((string/bytes (sprite 0)) 0) 32))
        (def offset (+ 0x4040 (* char 7 7)))
        (var i 0)
        (loop [pixel :in (tuple/slice sprite 1)]
          (poke (+ offset i) pixel)
          (++ i))))

(defn init []
  (load-sprites sprites))

(var n 0)
(var steps 0)
(defn step []
  (++ steps)
  (if (< n (length chs))
    (do
      (color (+ (* (bkg n) 16) (clr n)))
      (fill 0 0 width height (chs n))
      (if (= (% steps 5) 0) (++ n)))))
