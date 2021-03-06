#include "mpi.h"
#include "petsc.h"
#include "sys.h"

#include "parUtils.h"
#include "oda.h"


#include <execinfo.h>

#include <cxxabi.h>

#include "externVars.h"
#include "octreeStatistics.h"
#include "testUtils.h"
#include "genPts_par.h"

#include "dtypes.h"

#include "dynamicPartition.h"

#include "sfcSort.h"

//Don't want time to be synchronized. Need to check load imbalance.
#ifdef MPI_WTIME_IS_GLOBAL
#undef MPI_WTIME_IS_GLOBAL
#endif


/** Print a demangled stack backtrace of the caller function to FILE* out. */
//void handler (int sig) {
//  int rank;
//  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
//
//  // char fname[256];
//  // sprintf(fname, "trace%.2d", rank);
//  // FILE *out = fopen(fname, "w");
//  unsigned int max_frames = 63;
//
//  // if (!rank) {
//    printf("%s---------------------------------%s\n", RED, NRM);
//    printf("%sError:%s signal %d:\n", RED, NRM, sig);
//    printf("%s---------------------------------%s\n", RED, NRM);
//    printf("\n%s======= stack trace =======%s\n", GRN, NRM);
//  // }
//
//  // fprintf(out, "======= stack trace =======\n");
//
//  // storage array for stack trace address data
//  void *addrlist[max_frames + 1];
//
//  // retrieve current stack addresses
//  int addrlen = backtrace(addrlist, sizeof(addrlist) / sizeof(void *));
//
//  if (addrlen == 0) {
//    // if (!rank)
//    fprintf(stderr, "%s  <empty, possibly corrupt>%s\n",RED, NRM);
//
//    // fprintf(out, "    <empty, possibly corrupt>\n");
//    return;
//  }
//
//  // resolve addresses into strings containing "filename(function+address)",
//  // this array must be free()-ed
//  char **symbollist = backtrace_symbols(addrlist, addrlen);
//
//  // allocate string which will be filled with the demangled function name
//  size_t funcnamesize = 256;
//  char *funcname = (char *) malloc(funcnamesize);
//
//  // iterate over the returned symbol lines. skip the first, it is the
//  // address of this function.
//  for (int i = 1; i < addrlen; i++) {
//    char *begin_name = 0, *begin_offset = 0, *end_offset = 0;
//
//    // find parentheses and +address offset surrounding the mangled name:
//    // ./module(function+0x15c) [0x8048a6d]
//    for (char *p = symbollist[i]; *p; ++p) {
//      if (*p == '(')
//        begin_name = p;
//      else if (*p == '+')
//        begin_offset = p;
//      else if (*p == ')' && begin_offset) {
//        end_offset = p;
//        break;
//      }
//    }
//
//    if (begin_name && begin_offset && end_offset
//        && begin_name < begin_offset) {
//      *begin_name++ = '\0';
//      *begin_offset++ = '\0';
//      *end_offset = '\0';
//
//      // mangled name is now in [begin_name, begin_offset) and caller
//      // offset in [begin_offset, end_offset). now apply
//      // __cxa_demangle():
//
//      int status;
//      char *ret = abi::__cxa_demangle(begin_name,
//                                      funcname, &funcnamesize, &status);
//      if (status == 0) {
//        funcname = ret; // use possibly realloc()-ed string
//        // if (!rank)
//        printf("%s[%.2d]%s%s : %s%s%s : \n",RED,rank,YLW, symbollist[i], MAG, funcname,NRM);
//
//        // fprintf(out, "%s : %s : ", symbollist[i], funcname);
//      }
//      else {
//        // demangling failed. Output function name as a C function with
//        // no arguments.
//        // if (!rank)
//        printf("%s[%.2d]%s%s : %s%s()%s : \n", RED,rank, YLW, symbollist[i], GRN,begin_name, NRM);
//
//        // fprintf(out, "%s : %s() : ", symbollist[i], begin_name);
//      }
//      size_t p = 0;
//      char syscom[256];
//      while(symbollist[i][p] != '(' && symbollist[i][p] != ' ' && symbollist[i][p] != 0)
//        ++p;
//
//      sprintf(syscom,"addr2line %p -e %.*s", addrlist[i], p, symbollist[i]);
//      //last parameter is the file name of the symbol
//      system(syscom);
//    }
//    else {
//      // couldn't parse the line? print the whole line.
//      // if (!rank)
//      printf("%sCouldn't Parse:%s  %s\n", RED, NRM, symbollist[i]);
//
//      // fprintf(out, "  %s\n", symbollist[i]);
//    }
//  }
//
//  free(funcname);
//  free(symbollist);
//  // fclose(out);
//
//  exit(1);
//}

int main(int argc, char **argv) {


  int size, rank;
  bool incCorner = 1;
  char bFile[50];
  char ptsFileName[256];
  bool compressLut = false;

  unsigned int ptsLen;
  unsigned int maxNumPts = 1;
  unsigned int dim = 3;
  unsigned int maxDepth = atoi(argv[6]);
  double gSize[3];
  //initializeHilbetTable(2);


/*
#ifdef HILBERT_ORDERING
  G_MAX_DEPTH = maxDepth;
  G_dim = dim;
#endif
 */


  _InitializeHcurve(dim );

  double localTime, totalTime;
  double startTime, endTime;
  DendroIntL localSz, totalSz;
  std::vector<double> pts;
  std::vector<ot::TreeNode> linOct, balOct;
  bool morton_based_bal=true;


  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " inpfile slack [0.0,1.0] genPts[0 or 1] numPts_total num_pseudo_proc max_depth" <<std::endl;
    return -1;
  }

  double slack=atof(argv[2]);
  int enable_pts_io=atoi(argv[3]);
  long grainSize =atoi(argv[4]);
  int num_pseudo_proc=atoi(argv[5]);




  PetscInitialize(&argc, &argv, "options.hs", NULL);
  ot::RegisterEvents();
  ot::DA_Initialize(MPI_COMM_WORLD);
  
  // unsigned int num_pseudo_proc=1;


   //signal(SIGSEGV, handler);   // install our handler
   //signal(SIGTERM, handler);   // install our handler

#ifdef PETSC_USE_LOG
  int stages[3];
  PetscLogStageRegister("P2O.", &stages[0]);
  PetscLogStageRegister("Bal", &stages[1]);
  PetscLogStageRegister("ODACreate", &stages[2]);
  PetscErrorPrintf = PetscErrorPrintfNone;
#endif



  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  std::vector<ot::TreeNode> pSortedNodes;
  std::vector<ot::TreeNode> pConstructedNodes;
  std::vector<ot::TreeNode> pBalancedNodes;
  ot::TreeNode root(0,0,0,0,m_uiDim,maxDepth);
  //std::cout<<"Com Size:"<<size<<std::endl;

  if (!rank) {
    std::cout << BLU << "===============================================" << NRM << std::endl;
    std::cout << " Input Parameters"  << std::endl;
    std::cout << " Input File Prefix:"<<argv[1]  << std::endl;
    std::cout << " Slack Parameter::"<<slack  << std::endl;
    std::cout << " Gen Pts files:: "<< enable_pts_io  << std::endl;
    std::cout << " Number of Points per Process:: " << grainSize << std::endl;
    std::cout << " Max Depth:"<<maxDepth<<std::endl;
    std::cout << " Number of psuedo Processors:: "<<num_pseudo_proc<<std::endl;
    std::cout << BLU << "===============================================" << NRM << std::endl;
  }




  if(enable_pts_io==1)
  {
    genGauss(0.1,grainSize,dim,argv[1],MPI_COMM_WORLD);

  }


  sprintf(ptsFileName, "%s%d_%d.pts", argv[1], rank, size);
  //std::cout<<"Attempt to Read "<<ptsFileName<<std::endl;

  //Read pts from files
  if (!rank) {
    std::cout << RED " Reading  " << argv[1] << NRM << std::endl; // Point size
  }
  ot::readPtsFromFile(ptsFileName, pts);

  if (!rank) {
    std::cout << GRN " Finished reading  " << argv[1] << NRM << std::endl; // Point size
  }

  ptsLen = pts.size();

  std::vector<ot::TreeNode> tmpNodes;
  for (int i = 0; i < ptsLen; i += 3) {
    if ((pts[i] > 0.0) &&
        (pts[i + 1] > 0.0)
        && (pts[i + 2] > 0.0) &&
        (((unsigned int) (pts[i] * ((double) (1u << maxDepth)))) < (1u << maxDepth)) &&
        (((unsigned int) (pts[i + 1] * ((double) (1u << maxDepth)))) < (1u << maxDepth)) &&
        (((unsigned int) (pts[i + 2] * ((double) (1u << maxDepth)))) < (1u << maxDepth))) {
      tmpNodes.push_back(ot::TreeNode((unsigned int) (pts[i] * (double) (1u << maxDepth)),
                                      (unsigned int) (pts[i + 1] * (double) (1u << maxDepth)),
                                      (unsigned int) (pts[i + 2] * (double) (1u << maxDepth)),
                                      maxDepth, dim, maxDepth));
    }
  }
  pts.clear();

  // treeNodesTovtk(tmpNodes, rank, "input_points");
  //std::cout << rank << "removeDuplicates" << std::endl;

 /* par::SFC_3D_TreeSort(tmpNodes,0.01,maxDepth,MPI_COMM_WORLD);
  assert(par::test::isSorted(tmpNodes,MPI_COMM_WORLD));*/


#ifdef TREE_SORT
  SFC::parSort::SFC_treeSort(tmpNodes,pSortedNodes,pConstructedNodes,pBalancedNodes,slack,maxDepth,root,ROOT_ROTATION,1,TS_REMOVE_DUPLICATES,NUM_NPES_THRESHOLD,MPI_COMM_WORLD);
#else
  par::removeDuplicates<ot::TreeNode>(tmpNodes, false,MPI_COMM_WORLD);
#endif
  
  linOct = tmpNodes;
  tmpNodes.clear();
  // std::cout << rank << "partition" << std::endl;
  par::partitionW<ot::TreeNode>(linOct, NULL, MPI_COMM_WORLD);
  assert(par::test::isUniqueAndSorted(linOct,MPI_COMM_WORLD));

  // treeNodesTovtk(linOct,rank,"par_1");
  // treeNodesTovtk(linOct, rank, "ip");

  // reduce and only print the total ...
  localSz = linOct.size();
  par::Mpi_Reduce<DendroIntL>(&localSz, &totalSz, 1, MPI_SUM, 0, MPI_COMM_WORLD);

  pts.resize(3 * (linOct.size()));
  ptsLen = (3 * (linOct.size()));
  for (int i = 0; i < linOct.size(); i++) {
    pts[3 * i] = (((double) (linOct[i].getX())) + 0.5) / ((double) (1u << maxDepth));
    pts[(3 * i) + 1] = (((double) (linOct[i].getY())) + 0.5) / ((double) (1u << maxDepth));
    pts[(3 * i) + 2] = (((double) (linOct[i].getZ())) + 0.5) / ((double) (1u << maxDepth));
  } //end for i
  linOct.clear();
  gSize[0] = 1.0;
  gSize[1] = 1.0;
  gSize[2] = 1.0;


  // ===============================================Points2Octree BEGIN===================================================================

  MPI_Barrier(MPI_COMM_WORLD);

 if (!rank) {
    std::cout << BLU << "===============================================" << NRM << std::endl;
    std::cout << RED " Starting Points to Octree" NRM << std::endl;
    std::cout << BLU << "===============================================" << NRM << std::endl;
  }



  startTime = MPI_Wtime();
#ifdef PETSC_USE_LOG
  PetscLogStagePush(stages[0]);
#endif
  ot::points2Octree(pts, gSize, linOct, dim, maxDepth, maxNumPts, MPI_COMM_WORLD);
#ifdef PETSC_USE_LOG
  PetscLogStagePop();
#endif
  endTime = MPI_Wtime();

  MPI_Barrier(MPI_COMM_WORLD);
  //treeNodesTovtk(linOct, rank, "bf_bal");
  //par::test::isComplete(linOct,MPI_COMM_WORLD);



  if (!rank) {
    std::cout << BLU << "===============================================" << NRM << std::endl;
    std::cout << RED " Ended Points to Octree" NRM << std::endl;
    std::cout << BLU << "===============================================" << NRM << std::endl;
  }



  // par::partitionW<ot::TreeNode>(linOct, NULL, MPI_COMM_WORLD);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
 // treeNodesTovtk(linOct, rank, "bfBalancing");

  DendroIntL total_boundary_faces=0;

  localTime = endTime - startTime;
  par::Mpi_Reduce<double>(&localTime, &totalTime, 1, MPI_MAX, 0, MPI_COMM_WORLD);
  if (!rank) {
    std::cout << GRN " P2n Time: " YLW << totalTime << NRM << std::endl;
  }



  // ===============================================Points2Octree END ===================================================================


  if (!rank) {
    std::cout << BLU << "===============================================" << NRM << std::endl;
    std::cout << RED " Octree Statistics Calculation Before Balancing" NRM << std::endl;
    std::cout << BLU << "===============================================" << NRM << std::endl;
  }

  double stat_bf_bal[3];
  calculateBoundaryFaces(linOct,num_pseudo_proc,stat_bf_bal,MPI_COMM_WORLD);
  total_boundary_faces=(int)stat_bf_bal[3]*num_pseudo_proc*size;


  if (!rank) {
    std::cout << BLU << "===============================================" << NRM << std::endl;
    std::cout << RED " Boundary Surfaces (min):"<<stat_bf_bal[0]<< NRM << std::endl;
    std::cout << RED " Boundary Surfaces (max):"<<stat_bf_bal[1]<< NRM << std::endl;
    std::cout << RED " Boundary Surfaces (mean):"<<stat_bf_bal[2]<< NRM << std::endl;
    std::cout << BLU << "===============================================" << NRM << std::endl;
  }
    // reduce and only print the total ...
  localSz = linOct.size();
 // std::cout<<"Local unbalanced oct: "<<rank<<"\t # of oct:"<<localSz<<std::endl;
  par::Mpi_Reduce<DendroIntL>(&localSz, &totalSz, 1, MPI_SUM, 0, MPI_COMM_WORLD);
  if (rank == 0) {
    std::cout << GRN " # of Unbalanced Octants: " YLW << totalSz << NRM << std::endl;
  }

  if (!rank) {
    double surf_volume_r=total_boundary_faces/(double)totalSz;
    std::cout << BLU << "===============================================" << NRM << std::endl;
    std::cout << RED " Surface to Volume ratio:" <<total_boundary_faces<<"/"<<totalSz<<"="<<surf_volume_r<<NRM << std::endl;
    std::cout << BLU << "===============================================" << NRM << std::endl;
  }

assert(par::test::isUniqueAndSorted(linOct,MPI_COMM_WORLD));

//
////// ================================================================== Balancing BEGIN============================================================
  pts.clear();

  //treeNodesTovtk(linOct, rank, "p2o_output");

  // =========== Balancing ============
  MPI_Barrier(MPI_COMM_WORLD);
  if (!rank) {
    std::cout << BLU << "===============================================" << NRM << std::endl;
    std::cout << RED " Starting 2:1 Balance" NRM << std::endl;
    std::cout << BLU << "===============================================" << NRM << std::endl;
  }


  startTime = MPI_Wtime();
#ifdef PETSC_USE_LOG
  PetscLogStagePush(stages[1]);
#endif
  ot::balanceOctree(linOct, balOct, dim, maxDepth, incCorner, MPI_COMM_WORLD, NULL, NULL);
#ifdef PETSC_USE_LOG
  PetscLogStagePop();
#endif
  endTime = MPI_Wtime();



  linOct.clear();
  // compute total inp size and output size
  localSz = balOct.size();
  localTime = endTime - startTime;
  par::Mpi_Reduce<DendroIntL>(&localSz, &totalSz, 1, MPI_SUM, 0, MPI_COMM_WORLD);
  par::Mpi_Reduce<double>(&localTime, &totalTime, 1, MPI_MAX, 0, MPI_COMM_WORLD);

 // std::cout<<"Local balanced oct: "<<rank<<"\t # of oct:"<<localSz<<std::endl;

  if (!rank) {
    std::cout << "# of Balanced Octants: " << totalSz << std::endl;
    std::cout << "bal Time: " << totalTime << std::endl;
  }

  treeNodesTovtk(balOct,rank,"balOct_dendro");


  double stat_af_bal[3];
  calculateBoundaryFaces(balOct,num_pseudo_proc,stat_af_bal,MPI_COMM_WORLD);

  if (!rank) {
    std::cout << BLU << "===============================================" << NRM << std::endl;
    std::cout << RED " Boundary Surfaces (min):"<<stat_af_bal[0]<< NRM << std::endl;
    std::cout << RED " Boundary Surfaces (max):"<<stat_af_bal[1]<< NRM << std::endl;
    std::cout << RED " Boundary Surfaces (mean):"<<stat_af_bal[2]<< NRM << std::endl;
    std::cout << BLU << "===============================================" << NRM << std::endl;
  }

// ================================================================== Balancing END================================================================
//
#ifdef HILBERT_ORDERING
  sprintf(ptsFileName, "%s%d_%d.oct", "bal_out_H_", rank, size);
#else
  sprintf(ptsFileName, "%s%d_%d.oct", "bal_out_M_", rank, size);
#endif
//  ot::writeNodesToFile_binary(ptsFileName,balOct);
//  treeNodesTovtk(balOct, rank, "bal_output");
// double res=slack/2.0;
//  while (slack < 0.5) {
//
//    if(!rank)
//    std::cout<<YLW<<"Slack Parameter:"<<slack<<NRM<<std::endl;
//
//    flexiblePartitionCalculation(balOct, slack, num_pseudo_proc, MPI_COMM_WORLD);
//    slack=slack+res;
//  }
  assert(par::test::isUniqueAndSorted(balOct,MPI_COMM_WORLD));
  assert(par::test::isComplete(balOct,MPI_COMM_WORLD));

// for(int i=0;i<balOct.size();i++)
// {
//   for(int j=0;j<balOct.size();j++)
//   {
//     if((balOct[i] < balOct[j]) && (balOct[i].getLevel()!=balOct[j].getLevel()))
//     {
//       assert(balOct[i].isAncestor(balOct[j]));
//     }
//   }
// }

    /*int totalOcts=0;
    int localOcts=balOct.size();

    MPI_Allreduce (&localOcts,&totalOcts,1,MPI_INT,MPI_SUM,MPI_COMM_WORLD);

    ot::TreeNode *balOctAll_ptr = NULL;
    std::vector<ot::TreeNode> BalOctsAll;

    ot::TreeNode* balOct_ptr=&(*(balOct.begin()));



    int *recvcounts = new int[size];
    int *displs = new int[size];

    //par::Mpi_Gather (&localOcts, recvcounts, 1, 0, MPI_COMM_WORLD);
    MPI_Allgather(&localOcts,1,MPI_INT,recvcounts,1,MPI_INT,MPI_COMM_WORLD);

*//*    for(int i=0;i<size;i++)
      std::cout<<" "<<recvcounts[i]<<" ";
      std::cout<<std::endl;*//*

    displs[0] = 0;
    for (int i=1; i<size; ++i)
      displs[i] = displs[i-1] + recvcounts[i-1]; // scan

*//*    for(int i=0;i<size;i++)
      std::cout<<" "<<displs[i]<<" ";
      std::cout<<std::endl;*//*


    if(!rank) {
      BalOctsAll.resize(totalOcts);
      balOctAll_ptr = &(*BalOctsAll.begin());
    }

    MPI_Gatherv(balOct_ptr,balOct.size(), par::Mpi_datatype<ot::TreeNode>::value(),
                  balOctAll_ptr, recvcounts, displs, par::Mpi_datatype<ot::TreeNode>::value(),0,MPI_COMM_WORLD);



    if(!rank) {
      std::cout<<"Total Balanced Octants:"<<BalOctsAll.size()<<std::endl;
      assert(BalOctsAll.size()==totalOcts);
      assert(seq::test::isUniqueAndSorted(BalOctsAll));
      assert(ot::test::isBalanced(3, maxDepth, "Balfail", BalOctsAll, incCorner, 1));
      //ot::test::isBalanced(3, maxDepth, "Balfail", BalOctsAll, incCorner, 1);




    }

    // Clean up ...
    delete [] displs;
    delete [] recvcounts;
    BalOctsAll.clear();*/



  if(!rank)
  {
    std::cout<<YLW<<"Balanced Octree Complete Unique and Sorted: OK"<<NRM<<std::endl;
  }


  //DynamicPartitioning(balOct,slack,MPI_COMM_WORLD);


  //==================ODA Meshing=================================

  if (!rank) {
    std::cout << BLU << "===============================================" << NRM << std::endl;
    std::cout << RED " Starting ODA Meshing" NRM << std::endl;
    std::cout << BLU << "===============================================" << NRM << std::endl;
  }
  //ODA ...
  MPI_Barrier(MPI_COMM_WORLD);

  startTime = MPI_Wtime();
#ifdef PETSC_USE_LOG
  PetscLogStagePush(stages[2]);
#endif
  ot::DA da(balOct, MPI_COMM_WORLD, MPI_COMM_WORLD, compressLut);
#ifdef PETSC_USE_LOG
  PetscLogStagePop();
#endif
  endTime = MPI_Wtime();

  //treeNodesTovtk(balOct,rank,"oda_octree");

//  balOct.clear();
//  // compute total inp size and output size
  localSz = da.getNodeSize();
  localTime = endTime - startTime;
  par::Mpi_Reduce<DendroIntL>(&localSz, &totalSz, 1, MPI_SUM, 0, MPI_COMM_WORLD);
  par::Mpi_Reduce<double>(&localTime, &totalTime, 1, MPI_MAX, 0, MPI_COMM_WORLD);

  if (!rank) {
    std::cout << "Total # Vertices: " << totalSz << std::endl;
    std::cout << "Time to build ODA: " << totalTime << std::endl;
  }

#ifdef HILBERT_ORDERING
  da.computeHilbertRotations();
#endif


  //! Quality of the partition ...
  DendroIntL maxNodeSize, minNodeSize,
      maxBdyNode, minBdyNode,
      maxIndepSize, minIndepSize,
      maxElementSize, minElementSize;

  localSz = da.getNodeSize();
  par::Mpi_Reduce<DendroIntL>(&localSz, &maxNodeSize, 1, MPI_MAX, 0, MPI_COMM_WORLD);
  par::Mpi_Reduce<DendroIntL>(&localSz, &minNodeSize, 1, MPI_MIN, 0, MPI_COMM_WORLD);

  localSz = da.getBoundaryNodeSize();
  par::Mpi_Reduce<DendroIntL>(&localSz, &maxBdyNode, 1, MPI_MAX, 0, MPI_COMM_WORLD);
  par::Mpi_Reduce<DendroIntL>(&localSz, &minBdyNode, 1, MPI_MIN, 0, MPI_COMM_WORLD);

  localSz = da.getElementSize();
  par::Mpi_Reduce<DendroIntL>(&localSz, &maxElementSize, 1, MPI_MAX, 0, MPI_COMM_WORLD);
  par::Mpi_Reduce<DendroIntL>(&localSz, &minElementSize, 1, MPI_MIN, 0, MPI_COMM_WORLD);

  localSz = da.getIndependentSize();
  par::Mpi_Reduce<DendroIntL>(&localSz, &maxIndepSize, 1, MPI_MAX, 0, MPI_COMM_WORLD);
  par::Mpi_Reduce<DendroIntL>(&localSz, &minIndepSize, 1, MPI_MIN, 0, MPI_COMM_WORLD);

  if (!rank) {
    std::cout << "Nodes          \t(" << minNodeSize << ", " << maxNodeSize << ")" << std::endl;
    std::cout << "Boundary Node  \t(" << minBdyNode << ", " << maxBdyNode << ")" << std::endl;
    std::cout << "Element Size   \t(" << minElementSize << ", " << maxElementSize << ")" << std::endl;
    std::cout << "Independent    \t(" << minIndepSize << ", " << maxIndepSize << ")" << std::endl;


  }



  if (!rank) {
    std::cout << GRN << "Finalizing ..." << NRM << std::endl;
  }

  //assert(oda::test::odaTest(balOct,da,MPI_COMM_WORLD));
  //assert(oda::test::odaLoopTestAll(da,MPI_COMM_WORLD));
  //assert(oda::test::odaLoopTestWritable(da,MPI_COMM_WORLD));


#ifdef HILBERT_ORDERING
  delete [] rotations;
  rotations=NULL;
  delete [] HILBERT_TABLE;
  HILBERT_TABLE=NULL;
#endif


  ot::DA_Finalize();
  PetscFinalize();







} //end function

