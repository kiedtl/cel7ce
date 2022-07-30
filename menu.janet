# vim: sw=2 ts=2 sts=2 expandtab

(var menu [
  [ " about" ]
  [ "reload" ]
  [ "resume" ]
  [ "  quit" ]
])
(var sel 0)

(defn draw-menu []
  (color 1)
  (c7put 1 1 "menu:")
  (loop [i :range [0 (length menu)]]
    (if (= sel i)
      (do
        (color 1)
        (c7put 1 (+ i 2) ">")
        (color 0x0D))
      (do
        (c7put 1 (+ i 2) " ")
        (color 0x0D)))
    (c7put 3 (+ i 2) ((menu i) 0) " ")))

(defn init []
  (draw-menu))

(defn keydown [k]
  (cond
    (or  (= k  "k") (= k "up"))
      (if (> sel 0) (-- sel))
    (or  (= k  "j") (= k "down"))
      (if (< sel (- (length menu) 1)) (++ sel)))
  (draw-menu))

(defn mouse [type _ _ y]
  (if (= type "motion")
    (do
      (def ry (math/floor y))
      (var nsel (- ry 2))
      (if (and (>= nsel 0) (< nsel (length menu)))
        (do
          (set sel nsel)))
      (draw-menu))))
