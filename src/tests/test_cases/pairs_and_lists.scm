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

(define reduce
  (lambda (f ident vals)
    (if (null? vals)
      ident
      (reduce f (f ident (car vals)) (cdr vals)))))

(display (reduce (lambda (x y) (+ x y 1)) 0 '(1 2 3 4 5)))
;; 20
