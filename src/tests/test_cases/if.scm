(display ((if #f + -) 3 (* 5 2)))
(newline)
;; -7

(display ((if (odd? (* 5 1)) + -) 3 (* 5 2)))
(newline)
;; 13
