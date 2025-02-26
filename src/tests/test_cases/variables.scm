((lambda (x)
   (display x)
   (newline)
   (set! x 5)
   (display x)
   (newline)
   x)
 2)
;; 2
;; 5

((lambda (x)
   (display x)
   (newline)
   (define y (lambda (z) (* x z)))
   (display (y 5))
   (newline)
   x)
 2)
;; 2
;; 10
