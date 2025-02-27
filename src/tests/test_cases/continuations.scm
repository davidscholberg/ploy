(display (if
           (call/cc (lambda (c) (c #f) #t))
           1
           2))
(newline)
;; 2

(define cont #f)
(define
  x
  (call/cc
    (lambda (c)
      (set! cont c)
      3)))
(display x)
(newline)
(if (> x 0)
  (cont (- x 1)))
;; 3
;; 2
;; 1
;; 0
