/* dscal.f -- translated by f2c (version of 23 April 1993  18:34:30).
   You must link the resulting object file with the libraries:
        -lf2c -lm   (in that order)
*/

#include "f2c.h"

/* Subroutine */ void dscal_(n, da, dx, incx)
integer *n;
doublereal *da, *dx;
integer *incx;
{
    /* Local variables */
    static integer i, m, nincx;

/*     scales a vector by a constant.                                   */
/*     uses unrolled loops for increment equal to one.                  */
/*     jack dongarra, linpack, 3/11/78.                                 */
/*     modified 3/93 to return if incx .le. 0.                          */
/*     modified 12/3/93, array(1) declarations changed to array(*)      */

    if (*n <= 0 || *incx <= 0) {
        return;
    }
/*        code for increment equal to 1 */
    if (*incx == 1) {
        m = *n % 5;
        for (i = 0; i < m; ++i) {
            dx[i] *= *da;
        }
        for (i = m; i < *n; i += 5) {
            dx[i] *= *da; dx[i+1] *= *da; dx[i+2] *= *da; dx[i+3] *= *da; dx[i+4] *= *da;
        }
    }
/*        code for increment not equal to 1 */
    else {
        nincx = *n * *incx;
        for (i = 0; i < nincx; i += *incx) {
            dx[i] = *da * dx[i];
        }
    }
} /* dscal_ */
