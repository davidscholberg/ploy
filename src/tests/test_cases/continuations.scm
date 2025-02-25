(display (if
           (call/cc (lambda (c) (c #f) #t))
           1
           2))
(newline)
;; 2
