#ifndef DUNE_PDELAB_GRIDGLUELFS_HH
#define DUNE_PDELAB_GRIDGLUELFS_HH

#include "localfunctionspace.hh"
#include "gridgluetags.hh"
#include "gridgluegridfunctionspace.hh"
#include <dune/grid-glue/adapter/gridglue.hh>

namespace Dune {
  namespace PDELab {

    enum GridGlueContextTag
    {
      GFS_DOM0,
      GFS_DOM1,
      TRACE_DOM0,
      TRACE_DOM1
    };

    template<typename Context, GridGlueContextTag t>
    struct GridGlueContext
    {
      GridGlueContext(Context & c) : context(c) {}
      const Context & context;
      enum { tag = t };
    };

    template<typename Element>
    struct RemoteLFSComputeSizeVisitor :
      public ComputeSizeVisitor<Element>
    {
      typedef typename ComputeSizeVisitor<Element>::Entity Entity;

      template<typename Node, typename TreePath>
      void leaf(Node& node, TreePath treePath)
      {
        node.offset = this->offset;
        node.pfe = nullptr;
        node.n = Node::FESwitch::basis(node.finiteElement()).size();
        this->offset += node.n;
      }

      RemoteLFSComputeSizeVisitor(const Element& e,std::size_t offset = 0) :
        ComputeSizeVisitor<Element>(e,offset)
      {}
    };

    template<typename GFS,
             typename DI>
    struct GridGlueLocalFunctionSpaceBaseTraits
      : public LocalFunctionSpaceBaseTraits<GFS,DI>
    {
      typedef typename GFS::Traits::GridGlue GridGlue;
    };

#define GRIDGLUELFSMIXIN
    // local function space for a power grid function space
    template<typename GFS, typename RootGFS, typename LFS0, typename LFS1>
    class GridGlueLocalFunctionSpaceNode :
      public TypeTree::VariadicCompositeNode<LFS0,
                                             LFS1
#ifdef GRIDGLUELFSMIXIN
                                             ,
                                             // Mixin RemoteLFSes
                                             typename Dune::PDELab::TypeTree::TransformTree<typename GFS::template Child<0>::Type,
                                                                                            Dune::PDELab::gfs_to_remote_lfs<RootGFS> >::Type,
                                             typename Dune::PDELab::TypeTree::TransformTree<typename GFS::template Child<1>::Type,
                                                                                            Dune::PDELab::gfs_to_remote_lfs<RootGFS> >::Type
#endif
                                             >
      , public LocalFunctionSpaceBaseNode<GFS,typename gfs_to_lfs<RootGFS>::DOFIndex>
    {
      typedef GridGlueLocalFunctionSpaceNode<GFS, RootGFS, LFS0, LFS1> This;
      typedef typename gfs_to_lfs<RootGFS>::DOFIndex DOFIndex;
      typedef LocalFunctionSpaceBaseNode<GFS,DOFIndex> BaseT;
      typedef TypeTree::VariadicCompositeNode<LFS0,
                                              LFS1
#ifdef GRIDGLUELFSMIXIN
                                              ,
                                              typename Dune::PDELab::TypeTree::TransformTree<typename GFS::template Child<0>::Type,
                                                                                             Dune::PDELab::gfs_to_remote_lfs<RootGFS> >::Type,
                                              typename Dune::PDELab::TypeTree::TransformTree<typename GFS::template Child<1>::Type,
                                                                                             Dune::PDELab::gfs_to_remote_lfs<RootGFS> >::Type
#endif
                                              > TreeNode;

      template<typename>
      friend struct PropagateGlobalStorageVisitor;

      template<typename>
      friend struct ClearSizeVisitor;

      template<typename>
      friend struct ComputeSizeVisitor;

      template<typename>
      friend struct FillIndicesVisitor;

      template<typename ChildGFS>
      static
      Dune::shared_ptr<typename Dune::PDELab::TypeTree::TransformTree<ChildGFS,Dune::PDELab::gfs_to_remote_lfs<RootGFS> >::Type>
      createRemoteLocalFunctionSpace(Dune::shared_ptr<const ChildGFS> cgfs)
      {
        Dune::PDELab::gfs_to_remote_lfs<RootGFS> trafo;
        return Dune::PDELab::TypeTree::TransformTree<ChildGFS,Dune::PDELab::gfs_to_remote_lfs<RootGFS> >::transform_storage(cgfs, trafo);
      }

    public:
      typedef GridGlueLocalFunctionSpaceBaseTraits<GFS,DOFIndex> Traits;
      typedef PowerLocalFunctionSpaceTag ImplementationTag;
      typedef typename GFS::Traits::GridGlue GridGlue;
      typedef typename GridGlue::Grid0Patch::GridView::template Codim<0>::Entity Patch0Element;
      typedef typename GridGlue::Grid1Patch::GridView::template Codim<0>::Entity Patch1Element;
      typedef typename GridGlue::Intersection CouplingIntersection;

      //! \brief initialize with grid function space
      template<typename Transformation>
      GridGlueLocalFunctionSpaceNode (shared_ptr<const GFS> gfs,
                                      const Transformation& t,
                                      Dune::shared_ptr<LFS0> lfs0,
                                      Dune::shared_ptr<LFS1> lfs1)
        : TreeNode(lfs0,lfs1
#ifdef GRIDGLUELFSMIXIN
          ,
                   createRemoteLocalFunctionSpace(
                     gfs->template childStorage<0>()),
                   createRemoteLocalFunctionSpace(
                     gfs->template childStorage<1>())
#endif
          )
        , BaseT(gfs)
      {}

      template<typename Transformation>
      GridGlueLocalFunctionSpaceNode (const GFS& gfs,
                                      const Transformation& t,
                                      Dune::shared_ptr<LFS0> lfs0,
                                      Dune::shared_ptr<LFS1> lfs1)
        : TreeNode(lfs0,lfs1
#ifdef GRIDGLUELFSMIXIN
          ,
                   createRemoteLocalFunctionSpace(
                     gfs.template childStorage<0>()),
                   createRemoteLocalFunctionSpace(
                     gfs.template childStorage<1>())
#endif
          )
        , BaseT(stackobject_to_shared_ptr(gfs))
      {}

      //! \brief bind local function space to one of the GridGlue contextes (sub-domain cell or remote intersection)
      // explicitly state the different options for the context definitions, so that we are sure to create the correct spaces
      void bind (const GridGlueContext<Patch0Element,GFS_DOM0>& c)
      {
        bind(*this,this->template child<0>(), c.context, 0);
      }
      void bind (const GridGlueContext<Patch1Element,GFS_DOM1>& c)
      {
        bind(*this,this->template child<1>(), c.context, 1);
      }
#ifdef GRIDGLUELFSMIXIN
      void bind (const GridGlueContext<CouplingIntersection,TRACE_DOM0>& c)
      {
        bind(*this,this->template child<2>(), c.context);
      }
      void bind (const GridGlueContext<CouplingIntersection,TRACE_DOM1>& c)
      {
        bind(*this,this->template child<3>(), c.context);
      }
#endif
    private:

      template<typename NodeType, typename ChildNodeType, GridGlueContextTag t>
      void bind (NodeType& node, ChildNodeType& child,
        const CouplingIntersection& rit)
      {
        assert(&node == this);
        // compute sizes
        RemoteLFSComputeSizeVisitor<CouplingIntersection> csv(rit);
        csv.pre(node,0);
        TypeTree::applyToTree(child,csv);
        csv.post(node,0);
      }
      template<typename NodeType, typename ChildNodeType>
      void bind (NodeType& node, ChildNodeType& child,
        const typename ChildNodeType::Traits::Element& e,
        int childIndex)
      {
        assert(&node == this);

        typedef typename ChildNodeType::Traits::Element Element;

        // compute sizes
        ComputeSizeVisitor<Element> csv(e);
        csv.pre(node,0);
        TypeTree::applyToTree(child,csv);
        csv.post(node,0);

        // initialize iterators and fill indices
        FillIndicesVisitor<Element> fiv(e);
        TypeTree::applyToTree(child,fiv);
        fiv.afterChild(node,child,0,childIndex);
      }
    };

    // register GridGlueGFS -> LocalFunctionSpace transformation (variadic version)
    template<typename SourceNode, typename RootGFS>
    struct gridglue_gfs_to_lfs_template
    {
      template<typename TC0, typename TC1, typename... DUMMY>
      struct result
      {
        typedef GridGlueLocalFunctionSpaceNode<SourceNode,RootGFS,TC0,TC1> type;
      };
    };
    template<typename GridGlueGridFunctionSpace, typename Params>
    Dune::PDELab::TypeTree::TemplatizedGenericVariadicCompositeNodeTransformation<
      GridGlueGridFunctionSpace,
      gfs_to_lfs<Params>,
      gridglue_gfs_to_lfs_template< GridGlueGridFunctionSpace,Params>::template result
      >
    registerNodeTransformation(GridGlueGridFunctionSpace* cgfs, gfs_to_lfs<Params>* t, GridGlueGridFunctionSpaceTag* tag);

  } // end PDELab
} // end Dune

#endif // DUNE_PDELAB_GRIDGLUELFS_HH