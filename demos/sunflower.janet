# vim: sw=2 ts=2 sts=2 expandtab

(def height 80)
(def width  80)
(def scale    1)

(def golden-angle 137.508)
(var spirals @[])

(def sprites [
  [ "%"
    0 1 1 1 1 0 0
    1 1 1 1 1 1 0
    1 1 1 1 1 1 0
    1 1 1 1 1 1 0
    1 1 1 1 1 1 0
    0 1 1 1 1 0 0
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

(defn draw-spiral [theta-a samples]
  (def coords @[])
  (defn add-coord [x y]
    (def dx (+ (/ width  2) (math/round (* x 1))))
    (def dy (+ (/ height 2) (math/round (* y 1))))
    (if (and (>= dx 0) (>= dy 0) (< dx width) (< dy height))
      (c7put dx dy "%"))
    (array/push coords [dy dx]))
  (loop [i :range-to [theta-a samples]]
    (def theta (* (/ golden-angle 180) math/pi i))
    (def r (math/sqrt theta))
    (add-coord (* r (math/cos theta))
               (* r (math/sin theta)))
  (array/push spirals coords)))

(def max-samples (* width (math/sqrt width)))
(def start-samples 20)

(defn init []
  (load-sprites sprites)
  (draw-spiral 1 start-samples))

(var n 0)
(defn step []
  (fill 0 0 width height " ")
  (draw-spiral 1 (+ start-samples (* n 4)))
  (++ n))
