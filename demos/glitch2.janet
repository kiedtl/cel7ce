# vim: sw=2 ts=2 sts=2 expandtab

(def title "±sÂ©d5Â@3â¤½ã;¿dâ4")
(def scale 4)
(def width 20)
(def height 20)

(def palette [
    [0x00 0x00 0x00] # 0
    [0xF1 0xF1 0xF1] # 1
    [0xF2 0xF2 0xF2] # 2
    [0xF3 0xF3 0xF3] # 3
    [0xF4 0xF4 0xF4] # 4
    [0xF5 0xF5 0xF5] # 5
    [0xF6 0xF6 0xF6] # 6
    [0xF7 0xF7 0xF7] # 7
    [0xF8 0xF8 0xF8] # 8
    [0xF9 0xF9 0xF9] # 9
    [0xFA 0xFA 0xFA] # A
    [0xFB 0xFB 0xFB] # B
    [0xFC 0xFC 0xFC] # C
    [0xFD 0xFD 0xFD] # D
    [0xFE 0xFE 0xFE] # E
])

(defn rand [n]
  (math/round (% (* (math/random) 1000) n)))

(defn // [arg1 & args]
  (var accm arg1)
  (each arg args
    (/= accm arg))
  (math/round accm))

(defn load-palette [palette d]
  (defn adjust [seg d]
    (+ (- seg 0xF0) (* d 16)))
  (var i 0)
  (loop [color :in palette]
    (poke (+ 0x4000 (+ (* i 4) 0)) (adjust (color 2) d))
    (poke (+ 0x4000 (+ (* i 4) 1)) (adjust (color 1) d))
    (poke (+ 0x4000 (+ (* i 4) 2)) (adjust (color 0) d))
    (++ i)))

(defn init []
  (load-palette palette 0xF)
  (fill 0 0 width height " ")
  (loop [i :range [0x4040 0x52a0]]
    (poke i (rand 2))))

(var steps 0)
(var n 0)
(defn step []
  (++ steps)
  (load-palette palette (// n 16))
  (var clr (+  1 (rand 13)))
  (def ch  (+ 20 (rand 96)))
  (loop [i :range [0 10]]
    (color clr)
    (fill (rand width) (rand height)
      (rand (// width 2)) (rand (// height 5))
      (string/from-bytes ch)))
  (if (and (= (% steps 4) 0) (<= n 20))
    (+= n 1)))
