(display '(6 . 3))
(newline)
;; (6 . 3)

(display '('6))
(newline)
;; ((quote 6))

(display (quote ((quote 6))))
(newline)
;; ((quote 6))

(display (cons 'a '(b c)))
(newline)
;; (a b c)

(display (cons '(1 2 3) 4))
(newline)
;; ((1 2 3) . 4)

(display (cons 1 (cons 2 (cons 3 4))))
(newline)
;; (1 2 3 . 4)

(display (car (cdr '(1 . (2 . 3)))))
(newline)
;; 2
