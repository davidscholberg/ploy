(display (if
           (call/cc (lambda (c) (c #f) #t))
           1
           2))
(newline)
;; 2

((lambda (cont x)
   (set!
     x
     (call/cc
       (lambda (c)
         (set! cont c)
         x)))
   (display x)
   (newline)
   (if (eqv? x 0)
     x
     (cont (- x 1))))
 #f
 3)
;; 3
;; 2
;; 1
;; 0
