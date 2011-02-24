#ifndef DUNE_PDELAB_STATIONARYLINEARPROBLEM_HH
#define DUNE_PDELAB_STATIONARYLINEARPROBLEM_HH

#include<dune/common/timer.hh>

namespace Dune {
  namespace PDELab {

    //===============================================================
    // A class for solving linear stationary problems.
    // It assembles the matrix, computes the right hand side and
    // solves the problem.
    // This is only a first vanilla implementation which has to be improved.
    //===============================================================

    template<class GOS, class LS, class V> 
    class StationaryLinearProblemSolver
    {
      typedef typename V::ElementType Real;
      typedef typename GOS::template MatrixContainer<Real>::Type M;
      typedef typename GOS::Traits::TrialGridFunctionSpace::template VectorContainer<Real>::Type W;

    public:

      StationaryLinearProblemSolver (const GOS& gos_, V& x_, LS& ls_, typename V::ElementType reduction_, typename V::ElementType mindefect_ = 1e-99)
        : gos(gos_), ls(ls_), x(&x_), reduction(reduction_), mindefect(mindefect_)
      {
      }

      StationaryLinearProblemSolver (const GOS& gos_, LS& ls_, V& x_, typename V::ElementType reduction_, typename V::ElementType mindefect_ = 1e-99)
        : gos(gos_), ls(ls_), x(&x_), reduction(reduction_), mindefect(mindefect_)
      {
      }

      StationaryLinearProblemSolver (const GOS& gos_, LS& ls_, typename V::ElementType reduction_, typename V::ElementType mindefect_ = 1e-99)
          : gos(gos_), ls(ls_), x(0), reduction(reduction_), mindefect(mindefect_)
      {
      }

      void apply (V& x_) {
        x = &x_;
        apply();
      }

      void apply ()
      {
        Dune::Timer watch;
        double timing;

        // assemble matrix; optional: assemble only on demand!
        watch.reset();

        M m(gos); 

        timing = watch.elapsed();
        // timing = gos.trialGridFunctionSpace().gridview().comm().max(timing);
        if (gos.trialGridFunctionSpace().gridview().comm().rank()==0)
          std::cout << "=== matrix setup (max) " << timing << " s" << std::endl;
        watch.reset();

        m = 0.0;
        gos.jacobian(*x,m);

        timing = watch.elapsed();
        // timing = gos.trialGridFunctionSpace().gridview().comm().max(timing);
        if (gos.trialGridFunctionSpace().gridview().comm().rank()==0)
          std::cout << "=== matrix assembly (max) " << timing << " s" << std::endl;

        // assemble residual
        watch.reset();

        W r(gos.testGridFunctionSpace(),0.0);
        gos.residual(*x,r);  // residual is additive

        timing = watch.elapsed();
        // timing = gos.trialGridFunctionSpace().gridview().comm().max(timing);
        if (gos.trialGridFunctionSpace().gridview().comm().rank()==0)
          std::cout << "=== residual assembly (max) " << timing << " s" << std::endl;

        typename V::ElementType defect = ls.norm(r);

        // compute correction
        watch.reset();
        V z(gos.trialGridFunctionSpace(),0.0);
        typename V::ElementType red = std::min(reduction,defect/mindefect);
        std::cout << "=== solving (reduction: " << red << ") ";
        ls.apply(m,z,r,red); // solver makes right hand side consistent
        timing = watch.elapsed();
        // timing = gos.trialGridFunctionSpace().gridview().comm().max(timing);
        std::cout << "=== solving " << timing << " s" << std::endl;

        // and update
        *x -= z;
      }

    private:
      const GOS& gos;
      LS& ls;
      V* x;
      typename V::ElementType reduction;
      typename V::ElementType mindefect;
    };

  } // namespace PDELab
} // namespace Dune

#endif
