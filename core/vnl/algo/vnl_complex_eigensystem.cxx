// This is vxl/vnl/algo/vnl_complex_eigensystem.cxx
/*
  fsm@robots.ox.ac.uk
*/
#include "vnl_complex_eigensystem.h"

#include <vcl_cassert.h>
#include <vcl_iostream.h>

#include <vnl/vnl_matlab_print.h>
#include <vnl/vnl_complexify.h>
#include <vnl/algo/vnl_netlib.h> // zgeev_()

void vnl_complex_eigensystem::compute(vnl_matrix<vcl_complex<double> > const & A,
                                      bool right,
                                      bool left)
{
  A.assert_size(N, N);

  A.assert_finite();
  assert(! A.is_zero());

  if (right)
    R.resize(N, N);
  if (left)
    L.resize(N, N);

  //
  // Remember that fortran matrices and C matrices are transposed
  // relative to each other. Moreover, the documentation for zgeev
  // says that left eigenvectors u satisfy u^h A = lambda u^h,
  // where ^h denotes adjoint (conjugate transpose).
  // So we pass our left eigenvector storage as their right
  // eigenvector storage and vice versa.
  // But then we also have to conjugate our R after calling the routine.
  //
  vnl_matrix<vcl_complex<double> > tmp(A);

  int work_space=10*N;
  vnl_vector<vcl_complex<double> > work(work_space);

  int rwork_space=2*N;
  vnl_vector<double> rwork(rwork_space);

  int info;
  int tmpN = N;
  zgeev_(right ? "V" : "N",          // jobvl
         left  ? "V" : "N",          // jobvr
         &tmpN,                      // n
         tmp.data_block(),           // a
         &tmpN,                      // lda
         W.data_block(),             // w
         right ? R.data_block() : 0, // vl
         &tmpN,                      // ldvl
         left  ? L.data_block() : 0, // vr
         &tmpN,                      // ldvr
         work.data_block(),          // work
         &work_space,                // lwork
         rwork.data_block(),         // rwork
         &info                       // info
         );
  assert(tmpN == int(N));

  if (right) {
    // conjugate all elements of R :
    for (unsigned int i=0;i<N;i++)
      for (unsigned int j=0;j<N;j++)
        R(i,j) = vcl_conj( R(i,j) );
  }

  if (info == 0) {
    // success
  }
  else if (info < 0) {
    vcl_cerr << __FILE__ ": info = " << info << vcl_endl;
    vcl_cerr << __FILE__ ": " << (-info) << "th argument has illegal value" << vcl_endl;
    assert(false);
  }
  else /* if (info > 0) */ {
    vcl_cerr << __FILE__ ": info = " << info << vcl_endl;
    vcl_cerr << __FILE__ ": QR algorithm failed to compute all eigenvalues." << vcl_endl;
    vnl_matlab_print(vcl_cerr, A, "A", vnl_matlab_print_format_long);
    assert(false);
  }
}

//--------------------------------------------------------------------------------

//
vnl_complex_eigensystem::vnl_complex_eigensystem(vnl_matrix<vcl_complex<double> > const &A,
                                                 bool right,
                                                 bool left)
  : N(A.rows())
  // L and R are intentionally not initialized.
  , W(N)
{
  compute(A, right, left);
}

//
vnl_complex_eigensystem::vnl_complex_eigensystem(vnl_matrix<double> const &A_real,
                                                 vnl_matrix<double> const &A_imag,
                                                 bool right,
                                                 bool left)
  : N(A_real.rows())
  // L and R are intentionally not initialized.
  , W(N)
{
  A_real.assert_size(N,N);
  A_imag.assert_size(N,N);

  vnl_matrix<vcl_complex<double> > A(N,N);
  vnl_complexify(A_real.begin(), A_imag.begin(), A.begin(), A.size());

  compute(A, right, left);
}
