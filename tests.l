`(== 64 64)
(de hex2L (H)
   (unless (= '("0" "0") H) # dirty
      (make
         (for (L H L (cddr L))
            (link (hex (pack (car L) (cadr L)))) ) ) ) )
(de len (Str)
   (format
      (head 3 (cadr (split (chop Str) '_))) ) )
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
# S is 128 or 256
# OutLen for Variable tests
(de shake (D S OutLen)
   (default OutLen S)
   (let (O (/ OutLen 8)  DL (length D)  R)
(test
   (107 78 3 66 54 103 219 183 59 110 21 69 79 14 177 171 212 89 127 154 27 7 142 63 91 90 107 199)
   (sha3 NIL 224) )
(setq SHA3_Tests
   (quote
      "vectors/SHA3_224ShortMsg.rsp"
      "vectors/SHA3_224LongMsg.rsp"
      "vectors/SHA3_256ShortMsg.rsp"
      "vectors/SHA3_256LongMsg.rsp"
      "vectors/SHA3_384ShortMsg.rsp"
      "vectors/SHA3_384LongMsg.rsp"
      "vectors/SHA3_512ShortMsg.rsp"
      "vectors/SHA3_512LongMsg.rsp" ) )
(use (K C)
   (for V SHA3_Tests
      (let ShaLen (len V)
         (in V
            (until (eof)
               (and
                  (from "Msg = ")
                  (setq K (hex2L (line)))
                  (from "MD = ")
                  (setq C (hex2L (line)))
                  (test C (sha3 K ShaLen)) ) ) ) ) ) )

(msg 'ok)
(bye)

#{
// size 128 or 256
void shake(void *out, size_t outlen, unsigned size, const void *in, size_t len)
{
    shake_context ctx;
    shake_init(&ctx, size);
    shake_inject(&ctx, in, len);
    shake_flip(&ctx);
    shake_extract(&ctx, out, outlen);
}
}#
