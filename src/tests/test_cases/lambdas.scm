(display ((lambda (x) (* x x)) 5))
(newline)
;; 25

(display ((lambda (f) (f 5))
          (lambda (x) (* x x))))
(newline)
;; 25

(display ((((lambda (x)
              (lambda (y)
                (lambda (z) (* x y z))))
            5) 6) 2))
(newline)
;; 60
