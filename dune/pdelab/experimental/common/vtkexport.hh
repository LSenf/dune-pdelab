// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:

#ifndef DUNE_PDELAB_EXPERIMENTAL_COMMON_VTKEXPORT_HH
#define DUNE_PDELAB_EXPERIMENTAL_COMMON_VTKEXPORT_HH

#include<dune/grid/io/file/vtk/skeletonfunction.hh>

namespace Dune {
  namespace PDELab {

    //! wrap a BoundaryGridFunction for the VTKWriter
    /**
     * \tparam Func Type of the BoundaryGridFunction to wrap.
     *
     * \note There is one catch here: PDELabs BoundaryGridFunction is
     *       specified to receive an IntersectionGeometry object instead of an
     *       Intersection.  The class IntersectionGeometry has mostly the some
     *       methods as the class Intersection, there is however the method
     *       intersectionIndex() which returns the number of the Intersection
     *       within the Element in iteration order.  This is however a piece
     *       of information which we do not have available in this wrapper
     *       class and which would be quite costly to obtain.
     *       \par
     *       So instead of an IntersectionGeometry we provide an Intersection
     *       object to the BoundaryGridFunction.  If the BoundaryGridFunction
     *       actually requires a real IntersectionGeometry, this will not
     *       work.
     */
    template<typename Func>
    class VTKBoundaryGridFunctionAdapter
    {
      const Func& func;

    public:
      //! export Traits
      typedef VTK::SkeletonFunctionTraits
      < typename Func::Traits::GridViewType,
        typename Func::Traits::RangeFieldType> Traits;

      //! create an adapter object
      /**
       * \param func_ Reference to the function object to wrap.  This
       *              reference will be stored internally and should be valid
       *              for as long as this wrapper may be evaluated.
       */
      VTKBoundaryGridFunctionAdapter(const Func& func_)
        : func(func_)
      { }

      //! return number of components
      unsigned dimRange() const { return Func::Traits::dimRange; };

      //! evaluate function
      void evaluate(const typename Traits::Cell& c,
                    const typename Traits::Domain& xl,
                    typename Traits::Range& result) const
      {
        typename Func::Traits::RangeType val;
        func.evaluate(c, xl, val);
        result.assign(val.begin(), val.end());
      }
    };

  }
}

#endif // DUNE_PDELAB_EXPERIMENTAL_COMMON_VTKEXPORT_HH
