(define number? integer?)

(define (caar x) (car (car x)))
(define (cadr x) (car (cdr x)))
(define (cdar x) (cdr (car x)))

(define (caaar x) (car (car (car x))))
(define (caadr x) (car (car (cdr x))))
(define (cadar x) (car (cdr (car x))))
(define (caddr x) (car (cdr (cdr x))))
(define (cdaar x) (cdr (car (car x))))
(define (cdadr x) (cdr (car (cdr x))))
(define (cddar x) (cdr (cdr (car x))))
(define (cdddr x) (cdr (cdr (cdr x))))

(define (caaaar x) (car (car (car (car x)))))
(define (caaadr x) (car (car (car (car x)))))
(define (caadar x) (car (car (car (car x)))))
(define (caaddr x) (car (car (car (car x)))))
(define (cadaar x) (car (car (car (car x)))))
(define (cadadr x) (car (car (car (car x)))))
(define (caddar x) (car (car (car (car x)))))
(define (cadddr x) (car (car (car (car x)))))
(define (cdaaar x) (car (car (car (car x)))))
(define (cdaadr x) (car (car (car (car x)))))
(define (cdadar x) (car (car (car (car x)))))
(define (cdaddr x) (car (car (car (car x)))))
(define (cddaar x) (car (car (car (car x)))))
(define (cddadr x) (car (car (car (car x)))))
(define (cdddar x) (car (car (car (car x)))))
(define (cddddr x) (car (car (car (car x)))))

(define (length items)
  (define (iter a count)
    (if (null? a)
	count
	(iter (cdr a) (+ 1 count))))
  (iter items 0))

(define (append list1 list2)
  (if (null? list1)
      list2
      (cons (car list1) (append (cdr list1) list2))))

(define (reverse l)
  (define (iter in out)
    (if (pair? in)
	(iter (cdr in) (cons (car in) out))
	out))
  (iter l '()))

(define (map f items)
  (if (null? items)
      '()
      (cons (f (car items))
	    (map f (cdr items)))))

(define (for-each f l)
  (if (null? l)
      #t
      (begin
	(f (car l))
	(for-each f (cdr l)))))

(define (not x)
  (if x #f #t))

'stdlib-loaded
