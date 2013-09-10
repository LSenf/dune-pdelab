#ifndef DUNE_PDELAB_GRIDGLUEOPERATOR_HH
#define DUNE_PDELAB_GRIDGLUEOPERATOR_HH

#include "default/assembler.hh"
#include <dune/pdelab/gridfunctionspace/subspace.hh>

// #include <dune/pdelab/gridoperator/common/gridoperatorutilities.hh>
// #include <dune/pdelab/gridoperator/common/borderdofexchanger.hh>
// #include <dune/pdelab/gridfunctionspace/interpolate.hh>
// #include <dune/common/tupleutility.hh>

namespace Dune{
  namespace PDELab{

    //! Wrap intersection
    /**
     * \todo Please doc me!
     */
    template<typename I>
    class CouplingGeometry
    {
    public:
      typedef typename I::Geometry Geometry;
      typedef typename I::InsideLocalGeometry InsideLocalGeometry;
      typedef typename I::OutsideGeometry OutsideGeometry;
      typedef typename I::OutsideLocalGeometry OutsideLocalGeometry;
      typedef typename I::ctype ctype;

      typedef typename I::InsideEntity InsideEntity;
      typedef typename I::InsideEntityPointer InsideEntityPointer;

      typedef typename I::OutsideEntity OutsideEntity;
      typedef typename I::OutsideEntityPointer OutsideEntityPointer;

      enum {
        coorddim = I::coorddim,
        mydim = I::mydim,
        insidePatch = I::insidePatch,
        outsidePatch = I::outsidePatch
      };

      //! \todo Please doc me!
      CouplingGeometry (const I& i_)
        : i(i_)
      {}

      /*! @brief geometrical information about this intersection in local
        coordinates of the inside() entity.

        This method returns a Geometry object that provides a mapping from
        local coordinates of the intersection to local coordinates of the
        inside() entity.
      */
      InsideLocalGeometry geometryInInside () const
      {
        return i.geometryInInside();
      }

      /*! @brief geometrical information about this intersection in local
        coordinates of the outside() entity.

        This method returns a Geometry object that provides a mapping from
        local coordinates of the intersection to local coordinates of the
        outside() entity.
      */
      OutsideLocalGeometry geometryInOutside () const
      {
        return i.geometryInOutside();
      }

      /*! @brief geometrical information about this intersection in global coordinates.

        This method returns a Geometry object that provides a mapping from
        local coordinates of the intersection to global (world) coordinates.
      */
      Geometry geometry () const
      {
        return i.geometry();
      }

      OutsideGeometry geometryOutside () const
      {
        return i.geometryOutside();
      }

      //! Local number of codim 1 entity in the inside() Entity where intersection is contained in
      int indexInInside () const
      {
        return i.indexInInside ();
      }

      //! Local number of codim 1 entity in outside() Entity where intersection is contained in
      int indexInOutside () const
      {
        return i.indexInOutside ();
      }

      /*! @brief Return an outer normal (length not necessarily 1)

        The returned vector may depend on local position within the intersection.
      */
      Dune::FieldVector<ctype, coorddim> outerNormal (const Dune::FieldVector<ctype, mydim>& local) const
      {
        return i.outerNormal(local);
      }

      /*! @brief return outer normal scaled with the integration element
        @copydoc outerNormal
        The normal is scaled with the integration element of the intersection. This
        method is redundant but it may be more efficent to use this function
        rather than computing the integration element via intersectionGlobal().
      */
      Dune::FieldVector<ctype, coorddim> integrationOuterNormal (const Dune::FieldVector<ctype, mydim>& local) const
      {
        return i.integrationOuterNormal(local);
      }

      /*! @brief Return unit outer normal (length == 1)

        The returned vector may depend on the local position within the intersection.
        It is scaled to have unit length.
      */
      Dune::FieldVector<ctype, coorddim> unitOuterNormal (const Dune::FieldVector<ctype, mydim>& local) const
      {
        return i.unitOuterNormal(local);
      }

      /*! @brief Return unit outer normal (length == 1)

        The returned vector may depend on the local position within the intersection.
        It is scaled to have unit length.
      */
      Dune::FieldVector<ctype, coorddim> centerUnitOuterNormal () const
      {
        return i.centerUnitOuterNormal();
      }

      /*! @brief return EntityPointer to the Entity on the inside of this
        intersection. That is the Entity where we started this .
      */
      InsideEntityPointer inside() const
      {
        return i.inside();
      }

      /*! @brief return EntityPointer to the Entity on the outside of this
        intersection. That is the neighboring Entity.

        @warning Don't call this method if there is no neighboring Entity
        (neighbor() returns false). In this case the result is undefined.
      */
      OutsideEntityPointer outside() const
      {
        return i.outside();
      }

      //! \todo Please doc me!
      const I& intersection () const
      {
        return i;
      }

      unsigned int intersectionIndex() const
      {
        return i.index();
      }

    private:
      const I& i;
    };

    template<typename GFSU, typename GFSV,
             typename CU, typename CV>
    class GridGlueAssembler
    {
      GFSU gfsu_;
      GFSV gfsv_;
      const CU & cu_;
      const CV & cv_;

    public:
      GridGlueAssembler (const GFSU& gfsu, const GFSV& gfsv, const CU& cu, const CV& cv)
        : gfsu_(gfsu)
        , gfsv_(gfsv)
        , cu_(cu)
        , cv_(cv)
      {}

      //! Get the trial grid function space
      const GFSU& trialGridFunctionSpace() const
      {
        return gfsu_;
      }

      //! Get the test grid function space
      const GFSV& testGridFunctionSpace() const
      {
        return gfsv_;
      }

      template<class LocalAssemblerEngine>
      void assemble(LocalAssemblerEngine & assembler_engine) const
      {
        // Notify assembler engine about oncoming assembly
        assembler_engine.preAssembly();

        assemble<GFS_DOM0>(assembler_engine, gfsv_.gridGlue().template gridView<0>());
        assemble<GFS_DOM1>(assembler_engine, gfsv_.gridGlue().template gridView<1>());

        assert(& gfsu_.gridGlue() == & gfsv_.gridGlue());

        typedef LocalFunctionSpace<GFSU> LFSU;
        typedef LocalFunctionSpace<GFSV> LFSV;
        LFSU lfsu(gfsu_);
        LFSV lfsv(gfsv_);
        LFSU rlfsu(gfsu_);
        LFSV rlfsv(gfsv_);

        typedef typename std::conditional<
          LocalAssemblerEngine::needs_constraints_caching,
          LFSIndexCache<LFSU,CU>,
          LFSIndexCache<LFSU,EmptyTransformation>
          >::type LFSUCache;

        typedef typename std::conditional<
          LocalAssemblerEngine::needs_constraints_caching,
          LFSIndexCache<LFSV,CV>,
          LFSIndexCache<LFSV,EmptyTransformation>
          >::type LFSVCache;

        LFSUCache lfsu_cache(lfsu,cu_);
        LFSVCache lfsv_cache(lfsv,cv_);
        LFSUCache rlfsu_cache(rlfsu,cu_);
        LFSVCache rlfsv_cache(rlfsv,cv_);

        const bool require_uv_skeleton = assembler_engine.requireUVSkeleton();
        const bool require_v_skeleton = assembler_engine.requireVSkeleton();
        // TODO
        // if(assembler_engine.assembleCoupling(...))
        //   continue;

        // Traverse remote intersections
        for (auto iit = gfsu_.gridGlue().template ibegin<0>();
             iit != gfsu_.gridGlue().template iend<0>();
             ++iit)
        {
          typedef typename GFSU::Traits::GridGlue::Grid0Patch::GridView::template Codim<0>::Entity Grid0Element;
          typedef typename GFSU::Traits::GridGlue::Grid1Patch::GridView::template Codim<0>::Entity Grid1Element;
          typedef GridGlueContext<Grid0Element,GFS_DOM0> Ctx0;
          typedef GridGlueContext<Grid1Element,GFS_DOM1> Ctx1;
          Ctx0 ctx0(*iit->inside());
          Ctx1 ctx1(*iit->outside());

          // DOM0, Trace0
          assemble_coupling(
            assembler_engine,
            ctx0, *iit,
            lfsu, lfsu_cache, lfsv, lfsv_cache, rlfsu, rlfsu_cache, rlfsv, rlfsv_cache);

          // DOM0, Trace1
          assemble_coupling(
            assembler_engine,
            ctx0, iit->flip(),
            lfsu, lfsu_cache, lfsv, lfsv_cache, rlfsu, rlfsu_cache, rlfsv, rlfsv_cache);

          // DOM1, Trace0
          assemble_coupling(
            assembler_engine,
            ctx1, *iit,
            lfsu, lfsu_cache, lfsv, lfsv_cache, rlfsu, rlfsu_cache, rlfsv, rlfsv_cache);

          // DOM1, Trace1
          assemble_coupling(
            assembler_engine,
            ctx1, iit->flip(),
            lfsu, lfsu_cache, lfsv, lfsv_cache, rlfsu, rlfsu_cache, rlfsv, rlfsv_cache);

          assemble_extra_pattern(assembler_engine,
            ctx0, ctx1, *iit, iit->flip(),
            lfsu, lfsu_cache, lfsv, lfsv_cache, rlfsu, rlfsu_cache, rlfsv, rlfsv_cache);
        }

        // Notify assembler engine that assembly is finished
        assembler_engine.postAssembly(gfsu_,gfsv_);

        // communicate();

        // for (auto iit = gfsu.gridGlue().template ibegin<1>();
        //      iit != gfsu.gridGlue().template iend<1>();
        //      ++iit)
        //     coupling_0_from_1(lfsu.template child<0>(),lfsv.template child<0>(),
        //         rlfsu.template child<1>(),rlfsv.template child<1>());
        //     coupling_1_from_0(lfsu.template child<1>(),lfsv.template child<1>(),
        //         rlfsu.template child<0>(),rlfsv.template child<0>());
        // }
      }

    private:

      template<class LocalAssemblerEngine,
               typename P0, typename P1, typename I0, typename I1,
               typename LFSV, typename LFSU, typename LFSV_C, typename LFSU_C>
      void assemble_extra_pattern(
        LocalAssemblerEngine & assembler_engine,
        const P0 & patch_ctx0, const P1 & patch_ctx1,
        const I0 & intersection0, const I1 & intersection1,
        LFSU & lfsu, LFSU_C & lfsu_cache,
        LFSV & lfsv, LFSV_C & lfsv_cache,
        LFSU & rlfsu, LFSU_C & rlfsu_cache,
        LFSV & rlfsv, LFSV_C & rlfsv_cache) const
      {
      }

      template<typename LA,
               typename P0, typename P1, typename I0, typename I1,
               typename LFSV, typename LFSU, typename LFSV_C, typename LFSU_C>
      void assemble_extra_pattern(
        DefaultLocalPatternAssemblerEngine<LA> & assembler_engine,
        const P0 & patch_ctx0, const P1 & patch_ctx1,
        const I0 & intersection0, const I1 & intersection1,
        LFSU & lfsu, LFSU_C & lfsu_cache,
        LFSV & lfsv, LFSV_C & lfsv_cache,
        LFSU & rlfsu, LFSU_C & rlfsu_cache,
        LFSV & rlfsv, LFSV_C & rlfsv_cache) const
      {
          assemble_extra_pattern(assembler_engine,
            patch_ctx0, patch_ctx1,
            lfsu, lfsu_cache, lfsv, lfsv_cache, rlfsu, rlfsu_cache, rlfsv, rlfsv_cache);
          assemble_extra_pattern(assembler_engine,
            patch_ctx1, patch_ctx0,
            lfsu, lfsu_cache, lfsv, lfsv_cache, rlfsu, rlfsu_cache, rlfsv, rlfsv_cache);
          assemble_extra_pattern(assembler_engine,
            intersection0, intersection0,
            lfsu, lfsu_cache, lfsv, lfsv_cache, rlfsu, rlfsu_cache, rlfsv, rlfsv_cache);
          assemble_extra_pattern(assembler_engine,
            intersection0, intersection1,
            lfsu, lfsu_cache, lfsv, lfsv_cache, rlfsu, rlfsu_cache, rlfsv, rlfsv_cache);
          assemble_extra_pattern(assembler_engine,
            intersection1, intersection1,
            lfsu, lfsu_cache, lfsv, lfsv_cache, rlfsu, rlfsu_cache, rlfsv, rlfsv_cache);
          assemble_extra_pattern(assembler_engine,
            intersection1, intersection0,
            lfsu, lfsu_cache, lfsv, lfsv_cache, rlfsu, rlfsu_cache, rlfsv, rlfsv_cache);
      }

      template<typename LA,
               typename C0, typename C1,
               typename LFSV, typename LFSU, typename LFSV_C, typename LFSU_C>
      void assemble_extra_pattern(
        DefaultLocalPatternAssemblerEngine<LA> & assembler_engine,
        const C0 & ctx0, const C1 & ctx1,
        LFSU & lfsu, LFSU_C & lfsu_cache,
        LFSV & lfsv, LFSV_C & lfsv_cache,
        LFSU & rlfsu, LFSU_C & rlfsu_cache,
        LFSV & rlfsv, LFSV_C & rlfsv_cache) const
      {
        Empty eg;
        typedef DefaultLocalPatternAssemblerEngine<LA> Engine;
        typename Engine::LocalPattern pattern;

        // Bind local test function space to element
        lfsv.bind( ctx0 );
        lfsv_cache.update();
        lfsu.bind( ctx1 );
        lfsu_cache.update();

        pattern.addLink(lfsu,0,lfsv,0);
        assembler_engine.onBindLFSUV(eg,lfsu_cache,lfsv_cache);
        assembler_engine.add_pattern(lfsv_cache, lfsu_cache, pattern);
        assembler_engine.onUnbindLFSUV(eg,lfsu_cache,lfsv_cache);
      }

      template<typename LocalAssemblerEngine,
               typename P, typename I,
               typename LFSV, typename LFSU, typename LFSV_C, typename LFSU_C>
      void assemble_coupling(
        LocalAssemblerEngine & assembler_engine,
        const P & patch_ctx, const I & intersection,
        LFSU & lfsu, LFSU_C & lfsu_cache,
        LFSV & lfsv, LFSV_C & lfsv_cache,
        LFSU & rlfsu, LFSU_C & rlfsu_cache,
        LFSV & rlfsv, LFSV_C & rlfsv_cache) const
      {
        Empty eg;
        CouplingGeometry<I> ig(intersection);

        // Bind local test function space to element
        lfsv.bind( patch_ctx );
        lfsv_cache.update();

        // Notify assembler engine about bind
        assembler_engine.onBindLFSV(eg,lfsv_cache);

        // Bind local trial function space to element
        lfsu.bind( patch_ctx );
        lfsu_cache.update();

        // Notify assembler engine about bind
        assembler_engine.onBindLFSUV(eg,lfsu_cache,lfsv_cache);

        // Load coefficients of local functions
        assembler_engine.loadCoefficientsLFSUInside(lfsu_cache);

        // Bind local test space to neighbor element
        rlfsv.bind( intersection );
        rlfsv_cache.update();

        // Notify assembler engine about binds
        assembler_engine.onBindLFSVOutside(ig,lfsv_cache,rlfsv_cache);

        // Skeleton integration
        assembler_engine.assembleVSkeleton(ig,lfsv_cache,rlfsv_cache);

        // TODO
        // if(require_uv_skeleton)
        {

          // Bind local trial space to neighbor element
          rlfsu.bind( intersection );
          rlfsu_cache.update();

          // Notify assembler engine about binds
          assembler_engine.onBindLFSUVOutside(ig,
            lfsu_cache,lfsv_cache,
            rlfsu_cache,rlfsv_cache);

          // Load coefficients of local functions
          assembler_engine.loadCoefficientsLFSUOutside(rlfsu_cache);

          // Skeleton integration
          assembler_engine.assembleUVSkeleton(ig,lfsu_cache,lfsv_cache,rlfsu_cache,rlfsv_cache);

          // Notify assembler engine about unbinds
          assembler_engine.onUnbindLFSUVOutside(ig,
            lfsu_cache,lfsv_cache,
            rlfsu_cache,rlfsv_cache);
        }

        // Notify assembler engine about unbinds
        assembler_engine.onUnbindLFSVOutside(ig,lfsv_cache,rlfsv_cache);

        // Notify assembler engine about unbinds
        assembler_engine.onUnbindLFSUV(eg,lfsu_cache,lfsv_cache);

        // Notify assembler engine about unbinds
        assembler_engine.onUnbindLFSV(eg,lfsv_cache);
      }

      template<GridGlueContextTag TAG, class LocalAssemblerEngine, typename GV>
      void assemble(LocalAssemblerEngine & assembler_engine, const GV & gv) const
      {
        typedef typename GV::Traits::template Codim<0>::Entity Element;
        typedef typename GV::Traits::template Codim<0>::Iterator ElementIterator;
        typedef typename GV::Traits::template Codim<0>::Entity Element;
        typedef typename GV::IntersectionIterator IntersectionIterator;
        typedef typename IntersectionIterator::Intersection Intersection;

        typedef LocalFunctionSpace<GFSU> LFSU;
        typedef LocalFunctionSpace<GFSV> LFSV;
        LFSU lfsu(gfsu_);
        LFSV lfsv(gfsv_);
        LFSU lfsun(gfsu_);
        LFSV lfsvn(gfsv_);

        typedef typename std::conditional<
          LocalAssemblerEngine::needs_constraints_caching,
          LFSIndexCache<LFSU,CU>,
          LFSIndexCache<LFSU,EmptyTransformation>
          >::type LFSUCache;

        typedef typename std::conditional<
          LocalAssemblerEngine::needs_constraints_caching,
          LFSIndexCache<LFSV,CV>,
          LFSIndexCache<LFSV,EmptyTransformation>
          >::type LFSVCache;

        LFSUCache lfsu_cache(lfsu,cu_);
        LFSVCache lfsv_cache(lfsv,cv_);
        LFSUCache lfsun_cache(lfsun,cu_);
        LFSVCache lfsvn_cache(lfsvn,cv_);

        // // Notify assembler engine about oncoming assembly
        // assembler_engine.preAssembly();

        // Map each cell to unique id
        ElementMapper<GV> cell_mapper(gv);

        // Extract integration requirements from the local assembler
        #warning disable skeleton terms for subdomains
        const bool require_uv_skeleton = false; // assembler_engine.requireUVSkeleton();
        const bool require_v_skeleton = false; // assembler_engine.requireVSkeleton();
        const bool require_uv_boundary = assembler_engine.requireUVBoundary();
        const bool require_v_boundary = assembler_engine.requireVBoundary();
        const bool require_uv_processor = assembler_engine.requireUVBoundary();
        const bool require_v_processor = assembler_engine.requireVBoundary();
        const bool require_uv_post_skeleton = assembler_engine.requireUVVolumePostSkeleton();
        const bool require_v_post_skeleton = assembler_engine.requireVVolumePostSkeleton();
        const bool require_skeleton_two_sided = assembler_engine.requireSkeletonTwoSided();

        // Traverse grid view
        for (ElementIterator it = gv.template begin<0>();
             it!=gv.template end<0>(); ++it)
        {
          // Compute unique id
          const typename GV::IndexSet::IndexType ids = cell_mapper.map(*it);

          ElementGeometry<Element> eg(*it);
          GridGlueContext<Element,TAG> ctx(*it);

          if(assembler_engine.assembleCell(eg))
            continue;

          // Bind local test function space to element
          lfsv.bind( ctx );
          lfsv_cache.update();

          // Notify assembler engine about bind
          assembler_engine.onBindLFSV(eg,lfsv_cache);

          // Volume integration
          assembler_engine.assembleVVolume(eg,lfsv_cache);

          // Bind local trial function space to element
          lfsu.bind( ctx );
          lfsu_cache.update();

          // Notify assembler engine about bind
          assembler_engine.onBindLFSUV(eg,lfsu_cache,lfsv_cache);

          // Load coefficients of local functions
          assembler_engine.loadCoefficientsLFSUInside(lfsu_cache);

          // Volume integration
          assembler_engine.assembleUVVolume(eg,lfsu_cache,lfsv_cache);

          // Skip if no intersection iterator is needed
          if (require_uv_skeleton || require_v_skeleton ||
            require_uv_boundary || require_v_boundary ||
            require_uv_processor || require_v_processor)
          {
            // Traverse intersections
            unsigned int intersection_index = 0;
            IntersectionIterator endit = gv.iend(*it);
            IntersectionIterator iit = gv.ibegin(*it);
            for(; iit!=endit; ++iit, ++intersection_index)
            {

              IntersectionGeometry<Intersection> ig(*iit,intersection_index);

              switch (IntersectionType::get(*iit))
              {
                case IntersectionType::skeleton:
                  // the specific ordering of the if-statements in the old code caused periodic
                  // boundary intersection to be handled the same as skeleton intersections
                case IntersectionType::periodic:
                  if (require_uv_skeleton || require_v_skeleton)
                  {
                    // compute unique id for neighbor

                    const typename GV::IndexSet::IndexType idn = cell_mapper.map(*(iit->outside()));

                    // Visit face if id is bigger
                    bool visit_face = ids > idn || require_skeleton_two_sided;

                    // unique vist of intersection
                    if (visit_face)
                    {
                      // Bind local test space to neighbor element
                      GridGlueContext<Element,TAG> ctxn(*(iit->outside()));
                      lfsvn.bind(ctxn);
                      lfsvn_cache.update();

                      // Notify assembler engine about binds
                      assembler_engine.onBindLFSVOutside(ig,lfsv_cache,lfsvn_cache);

                      // Skeleton integration
                      assembler_engine.assembleVSkeleton(ig,lfsv_cache,lfsvn_cache);

                      if(require_uv_skeleton){

                        // Bind local trial space to neighbor element
                        lfsun.bind(ctxn);
                        lfsun_cache.update();

                        // Notify assembler engine about binds
                        assembler_engine.onBindLFSUVOutside(ig,
                          lfsu_cache,lfsv_cache,
                          lfsun_cache,lfsvn_cache);

                        // Load coefficients of local functions
                        assembler_engine.loadCoefficientsLFSUOutside(lfsun_cache);

                        // Skeleton integration
                        assembler_engine.assembleUVSkeleton(ig,lfsu_cache,lfsv_cache,lfsun_cache,lfsvn_cache);

                        // Notify assembler engine about unbinds
                        assembler_engine.onUnbindLFSUVOutside(ig,
                          lfsu_cache,lfsv_cache,
                          lfsun_cache,lfsvn_cache);
                      }

                      // Notify assembler engine about unbinds
                      assembler_engine.onUnbindLFSVOutside(ig,lfsv_cache,lfsvn_cache);
                    }
                  }
                  break;

                case IntersectionType::boundary:
                  if(require_uv_boundary || require_v_boundary )
                  {

                    // Boundary integration
                    assembler_engine.assembleVBoundary(ig,lfsv_cache);

                    if(require_uv_boundary){
                      // Boundary integration
                      assembler_engine.assembleUVBoundary(ig,lfsu_cache,lfsv_cache);
                    }
                  }
                  break;

                case IntersectionType::processor:
                  if(require_uv_processor || require_v_processor )
                  {

                    // Processor integration
                    assembler_engine.assembleVProcessor(ig,lfsv_cache);

                    if(require_uv_processor){
                      // Processor integration
                      assembler_engine.assembleUVProcessor(ig,lfsu_cache,lfsv_cache);
                    }
                  }
                  break;
              } // switch

            } // iit
          } // do skeleton

          if(require_uv_post_skeleton || require_v_post_skeleton){
            // Volume integration
            assembler_engine.assembleVVolumePostSkeleton(eg,lfsv_cache);

            if(require_uv_post_skeleton){
              // Volume integration
              assembler_engine.assembleUVVolumePostSkeleton(eg,lfsu_cache,lfsv_cache);
            }
          }

          // Notify assembler engine about unbinds
          assembler_engine.onUnbindLFSUV(eg,lfsu_cache,lfsv_cache);

          // Notify assembler engine about unbinds
          assembler_engine.onUnbindLFSV(eg,lfsv_cache);

        } // it

        // // Notify assembler engine that assembly is finished
        // assembler_engine.postAssembly(gfsu_,gfsv_);

      }
    };

    /**
       \brief Standard grid operator implementation

       \tparam GFSU GridFunctionSpace for ansatz functions
       \tparam GFSV GridFunctionSpace for test functions
       \tparam MB The matrix backend to be used for representation of the jacobian
       \tparam DF The domain field type of the operator
       \tparam RF The range field type of the operator
       \tparam JF The jacobian field type
       \tparam CU   Constraints maps for the individual dofs (trial space)
       \tparam CV   Constraints maps for the individual dofs (test space)
       \tparam nonoverlapping_mode Switch for nonoverlapping grids

    */
    template<typename GFSU, typename GFSV, typename LOP,
             typename MB, typename DF, typename RF, typename JF,
             typename CU=Dune::PDELab::EmptyTransformation,
             typename CV=Dune::PDELab::EmptyTransformation,
             bool nonoverlapping_mode = false>
    class GridGlueOperator
    {
    public:

      //! The global assembler type
      typedef GridGlueAssembler<GFSU,GFSV,CU,CV> Assembler;

      //! The type of the domain (solution).
      typedef typename Dune::PDELab::BackendVectorSelector<GFSU,DF>::Type Domain;
      //! The type of the range (residual).
      typedef typename Dune::PDELab::BackendVectorSelector<GFSV,RF>::Type Range;
      //! The type of the jacobian.
      typedef typename Dune::PDELab::BackendMatrixSelector<MB,Domain,Range,JF>::Type Jacobian;

      //! The sparsity pattern container for the jacobian matrix
      typedef typename Jacobian::Pattern Pattern;

      //! The local assembler type
      typedef DefaultLocalAssembler<GridGlueOperator,LOP,nonoverlapping_mode>
      LocalAssembler;

      typedef NoDataBorderDOFExchanger<GridGlueOperator> BorderDOFExchanger;

      //! The grid operator traits
      typedef Dune::PDELab::GridOperatorTraits
      <GFSU,GFSV,MB,DF,RF,JF,CU,CV,Assembler,LocalAssembler> Traits;

      template <typename MFT>
      struct MatrixContainer{
        typedef typename Traits::Jacobian Type;
      };

      //! Constructor for non trivial constraints
      GridGlueOperator(const GFSU & gfsu_, const CU & cu_, const GFSV & gfsv_, const CV & cv_, LOP & lop_, const MB& mb_ = MB())
        : global_assembler(gfsu_,gfsv_,cu_,cv_)
        , dof_exchanger(make_shared<BorderDOFExchanger>(*this))
        , local_assembler(lop_, cu_, cv_,dof_exchanger)
        , backend(mb_)
      {}

      //! Constructor for empty constraints
      GridGlueOperator(const GFSU & gfsu_, const GFSV & gfsv_, LOP & lop_, const MB& mb_ = MB())
        : global_assembler(gfsu_,gfsv_)
        , dof_exchanger(make_shared<BorderDOFExchanger>(*this))
        , local_assembler(lop_,dof_exchanger)
        , backend(mb_)
      {}

      //! Get the trial grid function space
      const GFSU& trialGridFunctionSpace() const
      {
        return global_assembler.trialGridFunctionSpace();
      }

      //! Get the test grid function space
      const GFSV& testGridFunctionSpace() const
      {
        return global_assembler.testGridFunctionSpace();
      }

      //! Get dimension of space u
      typename GFSU::Traits::SizeType globalSizeU () const
      {
        return trialGridFunctionSpace().globalSize();
      }

      //! Get dimension of space v
      typename GFSV::Traits::SizeType globalSizeV () const
      {
        return testGridFunctionSpace().globalSize();
      }

      Assembler & assembler() { return global_assembler; }

      const Assembler & assembler() const { return global_assembler; }

      LocalAssembler & localAssembler() const { return local_assembler; }


      //! Visitor which is called in the method setupGridOperators for
      //! each tuple element.
      template <typename GridOperatorTuple>
      struct SetupGridOperator {
        SetupGridOperator()
          : index(0), size(Dune::tuple_size<GridOperatorTuple>::value) {}

        template <typename T>
        void visit(T& elem) {
          elem.localAssembler().doPreProcessing = index == 0;
          elem.localAssembler().doPostProcessing = index == size-1;
          ++index;
        }

        int index;
        const int size;
      };

      //! Method to set up a number of grid operators which are used
      //! in a joint assembling. It is assumed that all operators are
      //! specializations of the same template type
      template<typename GridOperatorTuple>
      static void setupGridOperators(GridOperatorTuple tuple)
      {
        Dune::ForEachValue<GridOperatorTuple> forEach(tuple);
        SetupGridOperator<GridOperatorTuple> setup_visitor;
        forEach.apply(setup_visitor);
      }

      //! Interpolate the constrained dofs from given function
      template<typename F, typename X>
      void interpolate (const X& xold, F& f, X& x) const
      {
        DUNE_THROW(NotImplemented, "interpolate doesn't work for GridGlueOperator");
        // // Interpolate f into grid function space and set corresponding coefficients
        // Dune::PDELab::interpolate(f,global_assembler.trialGridFunctionSpace(),x);

        // // Copy non-constrained dofs from old time step
        // Dune::PDELab::copy_nonconstrained_dofs(local_assembler.trialConstraints(),xold,x);
      }

      //! Fill pattern of jacobian matrix
      void fill_pattern(Pattern & p) const {
        typedef typename LocalAssembler::LocalPatternAssemblerEngine PatternEngine;
        PatternEngine & pattern_engine = local_assembler.localPatternAssemblerEngine(p);
        global_assembler.assemble(pattern_engine);
      }

      //! Assemble residual
      void residual(const Domain & x, Range & r) const {
        typedef typename LocalAssembler::LocalResidualAssemblerEngine ResidualEngine;
        ResidualEngine & residual_engine = local_assembler.localResidualAssemblerEngine(r,x);
        global_assembler.assemble(residual_engine);
      }

      //! Assembler jacobian
      void jacobian(const Domain & x, Jacobian & a) const {
        typedef typename LocalAssembler::LocalJacobianAssemblerEngine JacobianEngine;
        JacobianEngine & jacobian_engine = local_assembler.localJacobianAssemblerEngine(a,x);
        global_assembler.assemble(jacobian_engine);
      }

      //! Apply jacobian matrix without explicitly assembling it
      void jacobian_apply(const Domain & x, Range & r) const {
        typedef typename LocalAssembler::LocalJacobianApplyAssemblerEngine JacobianApplyEngine;
        JacobianApplyEngine & jacobian_apply_engine = local_assembler.localJacobianApplyAssemblerEngine(r,x);
        global_assembler.assemble(jacobian_apply_engine);
      }

      void make_consistent(Jacobian& a) const {
        // we assume to work on consistent meshes
      }

    private:
      Assembler global_assembler;
      shared_ptr<BorderDOFExchanger> dof_exchanger;

      mutable LocalAssembler local_assembler;
      MB backend;

    };

  } // end namespace PDELab
} // end namespace Dune

#endif // DUNE_PDELAB_GRIDGLUEOPERATOR_HH
