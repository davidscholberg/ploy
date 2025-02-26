(display ((if #f + -) 3 (* 5 2)))
(newline)
;; -7

(display ((if (odd? (* 5 1)) + -) 3 (* 5 2)))
(newline)
;; 13

(display (if (eqv? 1 #t) 1 2))
(newline)
;; 2

(display (if (eqv? 1 1) 1 2))
(newline)
;; 1

(display (if (eqv? 1 2) 1 2))
(newline)
;; 2
