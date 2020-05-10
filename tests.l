`(== 64 64)
(de sha3 (D S)
   (let (O (/ S 8)  DL (length D)  R)
      (native
         "libcurve9767.so.0"
         "sha3"
         NIL
         (cons 'R (cons (cons O 'B O)))
         S
         (if D (cons NIL (cons DL) D) 0)
         DL )
      R ) )
(test
   (107 78 3 66 54 103 219 183 59 110 21 69 79 14 177 171 212 89 127 154 27 7 142 63 91 90 107 199)
   (sha3 NIL 224) )



(msg 'ok)
(bye)
