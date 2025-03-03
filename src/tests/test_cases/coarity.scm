(display
  ((lambda (x)
     3
     (* x x))
   5))
(newline)
;; 25

(display
  ((lambda (x)
     3
     (* 3 3)
     ((lambda (y) y) x)
     (* x x))
   5))
(newline)
;; 25
