(define square (lambda (x)
                 (* x x)))
(display (square 5))
(newline)
;; 25

(define call-with-5 (lambda (f)
                      (f 5)))
(display (call-with-5 square))
(newline)
;; 25

(display ((((lambda (x)
              (lambda (y)
                (lambda (z) (* x y z))))
            5) 6) 2))
(newline)
;; 60
