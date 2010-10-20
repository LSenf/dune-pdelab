// -*- tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=8 sw=2 sts=2:
#ifndef DUNE_OVLPISTLSOLVERBACKEND_HH
#define DUNE_OVLPISTLSOLVERBACKEND_HH

#include <dune/common/deprecated.hh>
#include <dune/common/mpihelper.hh>

#include <dune/istl/owneroverlapcopy.hh>
#include <dune/istl/solvercategory.hh>
#include <dune/istl/operators.hh>
#include <dune/istl/solvers.hh>
#include <dune/istl/preconditioners.hh>
#include <dune/istl/scalarproducts.hh>
#include <dune/istl/paamg/amg.hh>
#include <dune/istl/paamg/pinfo.hh>
#include <dune/istl/io.hh>
#include <dune/istl/superlu.hh>

#include "../gridfunctionspace/constraints.hh"
#include "../gridfunctionspace/genericdatahandle.hh"
#include "../newton/newton.hh"
#include "istlvectorbackend.hh"
#include "parallelistlhelper.hh"

namespace Dune {
  namespace PDELab {

    //========================================================
    // Generic support for overlapping grids
    // (need to be used with appropriate constraints)
    //========================================================

    // operator that resets result to zero at constrained DOFS
    template<class CC, class M, class X, class Y>
    class OverlappingOperator : public Dune::AssembledLinearOperator<M,X,Y>
    {
    public:
      //! export types
      typedef M matrix_type;
      typedef X domain_type;
      typedef Y range_type;
      typedef typename X::field_type field_type;

      //redefine the category, that is the only difference
      enum {category=Dune::SolverCategory::overlapping};

      OverlappingOperator (const CC& cc_, const M& A)
        : cc(cc_), _A_(A)
      {}

      //! apply operator to x:  \f$ y = A(x) \f$
      virtual void apply (const X& x, Y& y) const
      {
        _A_.mv(x,y);
        Dune::PDELab::set_constrained_dofs(cc,0.0,y);
      }

      //! apply operator to x, scale and add:  \f$ y = y + \alpha A(x) \f$
      virtual void applyscaleadd (field_type alpha, const X& x, Y& y) const
      {
        _A_.usmv(alpha,x,y);
        Dune::PDELab::set_constrained_dofs(cc,0.0,y);
      }

      //! get matrix via *
      virtual const M& getmat () const
      {
        return _A_;
      }

    private:
      const CC& cc;
      const M& _A_;
    };

    // new scalar product assuming at least overlap 1
    // uses unique partitioning of nodes for parallelization
    template<class GFS, class X>
    class OverlappingScalarProduct : public Dune::ScalarProduct<X>
    {
    public:
      //! export types
      typedef X domain_type;
      typedef typename X::ElementType field_type;

      //! define the category
      enum {category=Dune::SolverCategory::overlapping};

      /*! \brief Constructor needs to know the grid function space
       */
      OverlappingScalarProduct (const GFS& gfs_, const ParallelISTLHelper<GFS>& helper_)
        : gfs(gfs_), helper(helper_)
      {}


      /*! \brief Dot product of two vectors.
        It is assumed that the vectors are consistent on the interior+border
        partition.
      */
      virtual field_type dot (const X& x, const X& y)
      {
        // do local scalar product on unique partition
        field_type sum = 0;
        for (typename X::size_type i=0; i<x.N(); ++i)
          for (typename X::size_type j=0; j<x[i].N(); ++j)
            sum += (x[i][j]*y[i][j])*helper.mask(i,j);

        // do global communication
        return gfs.gridview().comm().sum(sum);
      }

      /*! \brief Norm of a right-hand side vector.
        The vector must be consistent on the interior+border partition
      */
      virtual double norm (const X& x)
      {
        return sqrt(static_cast<double>(this->dot(x,x)));
      }

    private:
      const GFS& gfs;
      const ParallelISTLHelper<GFS>& helper;
    };

    // wrapped sequential preconditioner
    template<class CC, class GFS, class P>
    class OverlappingWrappedPreconditioner
      : public Dune::Preconditioner<typename P::domain_type,typename P::range_type>
    {
    public:
      //! \brief The domain type of the preconditioner.
      typedef typename P::domain_type domain_type;
      //! \brief The range type of the preconditioner.
      typedef typename P::range_type range_type;

      // define the category
      enum {
        //! \brief The category the preconditioner is part of.
        category=Dune::SolverCategory::overlapping
      };

      //! Constructor.
      OverlappingWrappedPreconditioner (const GFS& gfs_, P& prec_, const CC& cc_,
                                        const ParallelISTLHelper<GFS>& helper_)
        : gfs(gfs_), prec(prec_), cc(cc_), helper(helper_)
      {}

      /*!
        \brief Prepare the preconditioner.

        \copydoc Preconditioner::pre(domain_type&,range_type&)
      */
      virtual void pre (domain_type& x, range_type& b)
      {
        prec.pre(x,b);
      }

      /*!
        \brief Apply the precondioner.

        \copydoc Preconditioner::apply(domain_type&,const range_type&)
      */
      virtual void apply (domain_type& v, const range_type& d)
      {
        range_type dd(d);
        set_constrained_dofs(cc,0.0,dd);
        prec.apply(v,dd);
        Dune::PDELab::AddDataHandle<GFS,domain_type> adddh(gfs,v);
        if (gfs.gridview().comm().size()>1)
          gfs.gridview().communicate(adddh,Dune::All_All_Interface,Dune::ForwardCommunication);
      }

      /*!
        \brief Clean up.

        \copydoc Preconditioner::post(domain_type&)
      */
      virtual void post (domain_type& x)
      {
        prec.post(x);
      }

    private:
      const GFS& gfs;
      P& prec;
      const CC& cc;
      const ParallelISTLHelper<GFS>& helper;
    };


#if HAVE_SUPERLU
    // exact subdomain solves with SuperLU as preconditioner
    template<class GFS, class M, class X, class Y>
    class SuperLUSubdomainSolver : public Dune::Preconditioner<X,Y>
    {
      typedef typename M::BaseT ISTLM;

    public:
      //! \brief The domain type of the preconditioner.
      typedef X domain_type;
      //! \brief The range type of the preconditioner.
      typedef Y range_type;
      //! \brief The field type of the preconditioner.
      typedef typename X::ElementType field_type;


      // define the category
      enum {
        //! \brief The category the preconditioner is part of.
        category=Dune::SolverCategory::overlapping
      };

      /*! \brief Constructor.

        Constructor gets all parameters to operate the prec.
        \param A The matrix to operate on.
        \param n The number of iterations to perform.
        \param w The relaxation factor.
      */
      SuperLUSubdomainSolver (const GFS& gfs_, const M& A_)
        : gfs(gfs_), A(A_), solver(A_,false) // this does the decomposition
      {}

      /*!
        \brief Prepare the preconditioner.

        \copydoc Preconditioner::pre(X&,Y&)
      */
      virtual void pre (X& x, Y& b) {}

      /*!
        \brief Apply the precondioner.

        \copydoc Preconditioner::apply(X&,const Y&)
      */
      virtual void apply (X& v, const Y& d)
      {
        Dune::InverseOperatorResult stat;
        Y b(d); // need copy, since solver overwrites right hand side
        solver.apply(v,b,stat);
        Dune::PDELab::AddDataHandle<GFS,X> adddh(gfs,v);
        if (gfs.gridview().comm().size()>1)
          gfs.gridview().communicate(adddh,Dune::All_All_Interface,Dune::ForwardCommunication);
      }

      /*!
        \brief Clean up.

        \copydoc Preconditioner::post(X&)
      */
      virtual void post (X& x) {}

    private:
      const GFS& gfs;
      const M& A;
      Dune::SuperLU<ISTLM> solver;
    };

    // exact subdomain solves with SuperLU as preconditioner
    template<class GFS, class M, class X, class Y>
    class RestrictedSuperLUSubdomainSolver : public Dune::Preconditioner<X,Y>
    {
      typedef typename M::BaseT ISTLM;

    public:
      //! \brief The domain type of the preconditioner.
      typedef X domain_type;
      //! \brief The range type of the preconditioner.
      typedef Y range_type;
      //! \brief The field type of the preconditioner.
      typedef typename X::ElementType field_type;


      // define the category
      enum {
        //! \brief The category the preconditioner is part of.
        category=Dune::SolverCategory::overlapping
      };

      /*! \brief Constructor.

        Constructor gets all parameters to operate the prec.
        \param A The matrix to operate on.
        \param n The number of iterations to perform.
        \param w The relaxation factor.
      */
      RestrictedSuperLUSubdomainSolver (const GFS& gfs_, const M& A_,
                                        const ParallelISTLHelper<GFS>& helper_)
        : gfs(gfs_), A(A_), solver(A_,false), helper(helper_) // this does the decomposition
      {}

      /*!
        \brief Prepare the preconditioner.

        \copydoc Preconditioner::pre(X&,Y&)
      */
      virtual void pre (X& x, Y& b) {}

      /*!
        \brief Apply the precondioner.

        \copydoc Preconditioner::apply(X&,const Y&)
      */
      virtual void apply (X& v, const Y& d)
      {
        Dune::InverseOperatorResult stat;
        Y b(d); // need copy, since solver overwrites right hand side
        solver.apply(v,b,stat);
        helper.mask(v);
        Dune::PDELab::AddDataHandle<GFS,X> adddh(gfs,v);
        if (gfs.gridview().comm().size()>1)
          gfs.gridview().communicate(adddh,Dune::InteriorBorder_All_Interface,Dune::ForwardCommunication);
      }

      /*!
        \brief Clean up.

        \copydoc Preconditioner::post(X&)
      */
      virtual void post (X& x) {}

    private:
      const GFS& gfs;
      const M& A;
      Dune::SuperLU<ISTLM> solver;
      const ParallelISTLHelper<GFS>& helper;
    };
#endif

    template<typename GFS>
    class OVLPScalarProductImplementation
    {
    public:
      OVLPScalarProductImplementation(const GFS& gfs_)
        : gfs(gfs_), helper(gfs_)
      {}

      /*! \brief Dot product of two vectors.
        It is assumed that the vectors are consistent on the interior+border
        partition.
      */
      template<typename X>
      typename X::ElementType dot (const X& x, const X& y) const
      {
        // do local scalar product on unique partition
        typename X::ElementType sum = 0;
        for (typename X::size_type i=0; i<x.N(); ++i)
          for (typename X::size_type j=0; j<x[i].N(); ++j)
            sum += (x[i][j]*y[i][j])*helper.mask(i,j);

        // do global communication
        return gfs.gridview().comm().sum(sum);
      }

      /*! \brief Norm of a right-hand side vector.
        The vector must be consistent on the interior+border partition
      */
       template<typename X>
      typename X::ElementType norm (const X& x) const
      {
        return sqrt(static_cast<double>(this->dot(x,x)));
      }

      const  ParallelISTLHelper<GFS>& parallelHelper()
      {
        return helper;
      }
      
    private:
      const GFS& gfs;
      ParallelISTLHelper<GFS> helper;
    };
    

    template<typename GFS, typename X>
    class OVLPScalarProduct
      : public ScalarProduct<X>
    {
    public:
      enum {category=Dune::SolverCategory::overlapping};
      OVLPScalarProduct(const OVLPScalarProductImplementation<GFS>& implementation_)
        : implementation(implementation_)
      {}
      virtual typename X::ElementType dot(const X& x, const X& y)
      {
        return implementation.dot(x,y);
      }
      
       virtual typename X::ElementType norm (const X& x)
      {
        return sqrt(static_cast<double>(this->dot(x,x)));
      }

    private:
      const OVLPScalarProductImplementation<GFS>& implementation;
    };
    
    template<class GFS, class C,
             template<class,class,class,int> class Preconditioner,
             template<class> class Solver>
    class ISTLBackend_OVLP_Base
      : public OVLPScalarProductImplementation<GFS>, public LinearResultStorage
    {
    public:
      /*! \brief make a linear solver object

        \param[in] gfs a grid function space
        \param[in] c a constraints object
        \param[in] maxiter maximum number of iterations to do
        \param[in] kssor number of SSOR steps to apply as inner iteration
        \param[in] verbose print messages if true
      */
      explicit ISTLBackend_OVLP_Base (const GFS& gfs_, const C& c_, unsigned maxiter_=5000,
                                            int steps_=5, int verbose_=1)
        : OVLPScalarProductImplementation<GFS>(gfs_), gfs(gfs_), c(c_), maxiter(maxiter_), steps(steps_), verbose(verbose_)
      {}
      
      /*! \brief solve the given linear system

        \param[in] A the given matrix
        \param[out] z the solution vector to be computed
        \param[in] r right hand side
        \param[in] reduction to be achieved
      */
      template<class M, class V, class W>
      void apply(M& A, V& z, W& r, typename V::ElementType reduction)
      {
        typedef Dune::PDELab::OverlappingOperator<C,M,V,W> POP;
        POP pop(c,A);
        typedef OVLPScalarProduct<GFS,V> PSP;
        PSP psp(*this);
        typedef Preconditioner<M,V,W,1> SeqPrec;
        SeqPrec seqprec(A,steps,1.0);
        typedef Dune::PDELab::OverlappingWrappedPreconditioner<C,GFS,SeqPrec> WPREC;
        WPREC wprec(gfs,seqprec,c,this->parallelHelper());
        int verb=0;
        if (gfs.gridview().comm().rank()==0) verb=verbose;
        Solver<V> solver(pop,psp,wprec,reduction,maxiter,verb);
        Dune::InverseOperatorResult stat;
        solver.apply(z,r,stat);
        res.converged  = stat.converged;
        res.iterations = stat.iterations;
        res.elapsed    = stat.elapsed;
        res.reduction  = stat.reduction;
      }
    private:
      const GFS& gfs;
      const C& c;
      unsigned maxiter;
      int steps;
      bool verbose;
    };
    

    template<class GFS, class C>
    class ISTLBackend_OVLP_BCGS_SSORk
      : public ISTLBackend_OVLP_Base<GFS,C,Dune::SeqSSOR, Dune::BiCGSTABSolver>
    {
      typedef Dune::PDELab::ParallelISTLHelper<GFS> PHELPER;

    public:
      /*! \brief make a linear solver object

        \param[in] gfs a grid function space
        \param[in] c a constraints object
        \param[in] maxiter maximum number of iterations to do
        \param[in] steps number of SSOR steps to apply as inner iteration
        \param[in] verbose print messages if true
      */
      explicit ISTLBackend_OVLP_BCGS_SSORk (const GFS& gfs, const C& c, unsigned maxiter=5000,
                                            int steps=5, int verbose=1)
        : ISTLBackend_OVLP_Base<GFS,C,Dune::SeqSSOR, Dune::BiCGSTABSolver>(gfs, c, maxiter, steps, verbose)
      {}
    };

    template<class GFS, class C>
    class ISTLBackend_OVLP_CG_SSORk
      : public ISTLBackend_OVLP_Base<GFS,C,Dune::SeqSSOR, Dune::CGSolver>
    {
      typedef Dune::PDELab::ParallelISTLHelper<GFS> PHELPER;

    public:
      /*! \brief make a linear solver object

        \param[in] gfs a grid function space
        \param[in] c a constraints object
        \param[in] maxiter maximum number of iterations to do
        \param[in] steps number of SSOR steps to apply as inner iteration
        \param[in] verbose print messages if true
      */
      explicit ISTLBackend_OVLP_CG_SSORk (const GFS& gfs, const C& c, unsigned maxiter=5000,
                                            int steps=5, int verbose=1)
        : ISTLBackend_OVLP_Base<GFS,C,Dune::SeqSSOR, Dune::CGSolver>(gfs, c, maxiter, steps, verbose)
      {}
    };

    template<class GFS, class C, template<typename> class Solver>
    class ISTLBackend_OVLP_SuperLU_Base
      : public OVLPScalarProductImplementation<GFS>, public LinearResultStorage
    {
      typedef Dune::PDELab::ParallelISTLHelper<GFS> PHELPER;

    public:
      /*! \brief make a linear solver object

        \param[in] gfs a grid function space
        \param[in] c a constraints object
        \param[in] maxiter maximum number of iterations to do
        \param[in] kssor number of SSOR steps to apply as inner iteration
        \param[in] verbose print messages if true
      */
      explicit ISTLBackend_OVLP_SuperLU_Base (const GFS& gfs_, const C& c_, unsigned maxiter_=5000,
                                              int verbose_=1)
        : OVLPScalarProductImplementation<GFS>(gfs_), gfs(gfs_), c(c_), maxiter(maxiter_), verbose(verbose_)
      {}

      /*! \brief solve the given linear system

        \param[in] A the given matrix
        \param[out] z the solution vector to be computed
        \param[in] r right hand side
        \param[in] reduction to be achieved
      */
      template<class M, class V, class W>
      void apply(M& A, V& z, W& r, typename V::ElementType reduction)
      {
        typedef Dune::PDELab::OverlappingOperator<C,M,V,W> POP;
        POP pop(c,A);
        typedef OVLPScalarProduct<GFS,V> PSP;
        PSP psp(*this);
#if HAVE_SUPERLU
        typedef Dune::PDELab::SuperLUSubdomainSolver<GFS,M,V,W> PREC;
        PREC prec(gfs,A);
        int verb=0;
        if (gfs.gridview().comm().rank()==0) verb=verbose;
        Solver<V> solver(pop,psp,prec,reduction,maxiter,verbose);
        Dune::InverseOperatorResult stat;
        solver.apply(z,r,stat);
        res.converged  = stat.converged;
        res.iterations = stat.iterations;
        res.elapsed    = stat.elapsed;
        res.reduction  = stat.reduction;
#else
        std::cout << "No superLU support, please install and configure it." << std::endl;
#endif
      }

    private:
      const GFS& gfs;
      const C& c;
      unsigned maxiter;
      int verbose;
    };

    template<class GFS, class C>
    class ISTLBackend_OVLP_BCGS_SuperLU
      : public ISTLBackend_OVLP_SuperLU_Base<GFS,C,Dune::BiCGSTABSolver>
    {
    public:
      
      /*! \brief make a linear solver object

        \param[in] gfs a grid function space
        \param[in] c a constraints object
        \param[in] maxiter maximum number of iterations to do
        \param[in] kssor number of SSOR steps to apply as inner iteration
        \param[in] verbose print messages if true
      */
      explicit ISTLBackend_OVLP_BCGS_SuperLU (const GFS& gfs_, const C& c_, unsigned maxiter_=5000,
                                              int verbose_=1)
        : ISTLBackend_OVLP_SuperLU_Base<GFS,C,Dune::BiCGSTABSolver>(gfs_,c_,maxiter_,verbose_)
      {}
    };
    
    template<class GFS, class C>
    class ISTLBackend_OVLP_CG_SuperLU
      : public ISTLBackend_OVLP_SuperLU_Base<GFS,C,Dune::CGSolver>
    {
    public:
      
      /*! \brief make a linear solver object

        \param[in] gfs a grid function space
        \param[in] c a constraints object
        \param[in] maxiter maximum number of iterations to do
        \param[in] kssor number of SSOR steps to apply as inner iteration
        \param[in] verbose print messages if true
      */
      explicit ISTLBackend_OVLP_CG_SuperLU (const GFS& gfs_, const C& c_, 
                                              unsigned maxiter_=5000,
                                              int verbose_=1)
        : ISTLBackend_OVLP_SuperLU_Base<GFS,C,Dune::CGSolver>(gfs_,c_,maxiter_,verbose_)
      {}
    };


    //! Solver to be used for explicit time-steppers with (block-)diagonal mass matrix
    template<class GFS>
    class ISTLBackend_OVLP_ExplicitDiagonal
      : public LinearResultStorage
    {
    public:
      /*! \brief make a linear solver object

        \param[in] maxiter maximum number of iterations to do
        \param[in] verbose print messages if true
      */
      explicit ISTLBackend_OVLP_ExplicitDiagonal (const GFS& gfs_)
        : gfs(gfs_)
      {}

      /*! \brief compute global norm of a vector

        \param[in] v the given vector
      */
      template<class V>
      typename V::ElementType norm(const V& v) const
      {
        dune_static_assert
          (AlwaysFalse<V>::value,
           "ISTLBackend_OVLP_ExplicitDiagonal::norm() should not be "
           "neccessary, so we skipped the implementation.  If you have a "
           "scenario where you need it, please implement it or report back to "
           "us.");
      }

      /*! \brief solve the given linear system

        \param[in] A the given matrix
        \param[out] z the solution vector to be computed
        \param[in] r right hand side
        \param[in] reduction to be achieved
      */
      template<class M, class V, class W>
      void apply(M& A, V& z, W& r, typename W::ElementType reduction)
      {
        Dune::SeqJac<M,V,W> jac(A,1,1.0);
        jac.pre(z,r);
        jac.apply(z,r);
        jac.post(z);
        if (gfs.gridview().comm().size()>1)
        {
          Dune::PDELab::CopyDataHandle<GFS,V> copydh(gfs,z);
          gfs.gridview().communicate(copydh,Dune::InteriorBorder_All_Interface,Dune::ForwardCommunication);
        }
        res.converged  = true;
        res.iterations = 1;
        res.elapsed    = 0.0;
        res.reduction  = reduction;
      }

    private:
      const GFS& gfs;
    };

    template<class GFS, int s, template<class,class,class,int> class SMI, template<class> class SOI>
    class ISTLBackend_AMG
    {
      typedef Dune::PDELab::ParallelISTLHelper<GFS> PHELPER;

    public:
      ISTLBackend_AMG(const GFS& gfs_, int smoothsteps=2,
                      unsigned maxiter_=5000, int verbose_=1)
        : gfs(gfs_), phelper(gfs), maxiter(maxiter_), steps(smoothsteps), verbose(verbose_)
      {}


      /*! \brief compute global norm of a vector

        \param[in] v the given vector
      */
      template<class V>
      typename V::ElementType norm (const V& v) const
      {
        typedef Dune::PDELab::OverlappingScalarProduct<GFS,V> PSP;
        PSP psp(gfs,phelper);
        return psp.norm(v);
      }

      /*! \brief solve the given linear system

        \param[in] A the given matrix
        \param[out] z the solution vector to be computed
        \param[in] r right hand side
        \param[in] reduction to be achieved
      */
      template<class M, class V>
      void apply(M& A, V& z, V& r, typename V::ElementType reduction)
      {
        typedef typename M::BaseT MatrixType;
        typedef typename BlockProcessor<GFS>::template AMGVectorTypeSelector<V>::Type
          VectorType;
        typedef typename CommSelector<s,Dune::MPIHelper::isFake>::type Comm;

        Comm oocc(gfs.gridview().comm());
        MatrixType& mat=A.base();
        phelper.createIndexSetAndProjectForAMG(mat, oocc);
        typedef Dune::Amg::CoarsenCriterion<Dune::Amg::SymmetricCriterion<MatrixType,
          Dune::Amg::FirstDiagonal> >
          Criterion;
        typedef SMI<MatrixType,VectorType,VectorType,1> Smoother;
        typedef Dune::BlockPreconditioner<VectorType,VectorType,Comm,Smoother> ParSmoother;
        typedef typename Dune::Amg::SmootherTraits<ParSmoother>::Arguments SmootherArgs;
        typedef Dune::OverlappingSchwarzOperator<MatrixType,VectorType,VectorType,Comm> Operator;
        typedef Dune::Amg::AMG<Operator,VectorType,ParSmoother,Comm> AMG;
        SmootherArgs smootherArgs;
        smootherArgs.iterations = 1;
        smootherArgs.relaxationFactor = 1;

        Criterion criterion(15,2000);
        criterion.setDefaultValuesIsotropic(GFS::Traits::GridViewType::Traits::Grid::dimension);
        criterion.setDebugLevel(verbose);
        Dune::OverlappingSchwarzScalarProduct<VectorType,Comm> sp(oocc);
        Operator oop(mat, oocc);
        //oocc.copyOwnerToAll(BlockProcessor<GFS>::getVector(r), BlockProcessor<GFS>::getVector(r));
        AMG amg=AMG(oop, criterion, smootherArgs, 1, steps, steps, false, oocc);

        Dune::InverseOperatorResult stat;
        int verb=0;
        if (gfs.gridview().comm().rank()==0) verb=verbose;

        SOI<VectorType> solver(oop,sp,amg,reduction,maxiter,verb);
        solver.apply(BlockProcessor<GFS>::getVector(z),BlockProcessor<GFS>::getVector(r),stat);
        res.converged  = stat.converged;
        res.iterations = stat.iterations;
        res.elapsed    = stat.elapsed;
        res.reduction  = stat.reduction;
        //oocc.copyOwnerToAll(BlockProcessor<GFS>::getVector(z), BlockProcessor<GFS>::getVector(z));
      }

      /*! \brief Return access to result data */
      const Dune::PDELab::LinearSolverResult<double>& result() const
      {
        return res;
      }

    private:
      const GFS& gfs;
      PHELPER phelper;
      Dune::PDELab::LinearSolverResult<double> res;
      unsigned maxiter;
      int steps;
      int verbose;
    };

    /**
     * @brief Parallel cojugate gradient solver preconditioned with AMG smoothed by SSOR
     * @tparam GFS The type of the grid functions space.
     * @tparam s The bits to use for the globale index.
     */
    template<class GFS, int s=96>
    class ISTLBackend_CG_AMG_SSOR
      : public ISTLBackend_AMG<GFS, s, Dune::SeqSSOR, Dune::CGSolver>
    {

    public:
      /**
       * @brief Constructor
       * @param gfs_ The grid function space used.
       * @param smoothsteps The number of steps to use for both pre and post smoothing.
       * @param maxiter_ The maximum number of iterations allowed.
       * @param verbose_ The verbosity level to use.
       */
      ISTLBackend_CG_AMG_SSOR(const GFS& gfs_,int smoothsteps=2,
                              unsigned maxiter_=5000, int verbose_=1)
        : ISTLBackend_AMG<GFS, s, Dune::SeqSSOR, Dune::CGSolver>(gfs_,smoothsteps, maxiter_,verbose_)
      {}
    };

    /**
     * @brief Parallel BiCGStab solver preconditioned with AMG smoothed by SSOR
     * @tparam GFS The type of the grid functions space.
     * @tparam s The bits to use for the globale index.
     */
    template<class GFS, int s=96>
    class ISTLBackend_BCGS_AMG_SSOR
      : public ISTLBackend_AMG<GFS, s, Dune::SeqSSOR, Dune::BiCGSTABSolver>
    {

    public:
      /**
       * @brief Constructor
       * @param gfs_ The grid function space used.
       * @param smoothsteps The number of steps to use for both pre and post smoothing.
       * @param maxiter_ The maximum number of iterations allowed.
       * @param verbose_ The verbosity level to use.
       */
      ISTLBackend_BCGS_AMG_SSOR(const GFS& gfs_, int smoothsteps=2,
                                unsigned maxiter_=5000, int verbose_=1)
        : ISTLBackend_AMG<GFS, s, Dune::SeqSSOR, Dune::BiCGSTABSolver>(gfs_,smoothsteps, maxiter_,verbose_)
      {}
    };

  } // namespace PDELab
} // namespace Dune

#endif