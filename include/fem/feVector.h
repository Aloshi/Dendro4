#ifndef __FE_VECTOR_H_
#define __FE_VECTOR_H_

#include <string>
#include "feVector.h"
#include "timeInfo.h"

template <typename T>
class feVector : public feVec {
  public:  

    enum stdElemType {
      ST_0,ST_1,ST_2,ST_3,ST_4,ST_5,ST_6,ST_7
    };

    enum exhaustiveElemType {
      //Order: 654321
      //YZ ZX Z XY Y X
      ET_N = 0,
      ET_Y = 2,
      ET_X = 1,
      ET_XY = 3,
      ET_Z = 8,
      ET_ZY = 10,
      ET_ZX = 9,
      ET_ZXY = 11,
      ET_XY_XY = 7,
      ET_XY_ZXY = 15,
      ET_YZ_ZY = 42,
      ET_YZ_ZXY = 43,
      ET_YZ_XY_ZXY = 47,
      ET_ZX_ZX = 25,
      ET_ZX_ZXY = 27,
      ET_ZX_XY_ZXY = 31,
      ET_ZX_YZ_ZXY = 59,
      ET_ZX_YZ_XY_ZXY = 63
    };

  feVector();  
  feVector(daType da);
  ~feVector();

  // operations.
  void setStencil(void* stencil);

  void setName(std::string name);

  virtual bool addVec(Vec _in, double scale=1.0, int indx = -1);

  virtual bool computeVec(Vec _in, Vec _out, double scale = 1.0);
  
  inline bool ElementalAddVec(int i, int j, int k, PetscScalar ***in, double scale) {
    return asLeaf().ElementalAddVec(i,j,k,in,scale);  
  }

  inline bool ElementalAddVec(unsigned int index, PetscScalar *in, double scale) {
    return asLeaf().ElementalAddVec(index, in, scale);  
  }

  inline bool ComputeNodalFunction(int i, int j, int k, PetscScalar ***in, PetscScalar ***out,double scale) {
    return asLeaf().ComputeNodalFunction(i,j,k,in,out,scale);  
  }

  inline bool ComputeNodalFunction(PetscScalar *in, PetscScalar *out,double scale) {
    return asLeaf().ComputeNodalFunction(in, out,scale);  
  }

  /**
 * @brief		Allows static polymorphism access to the derived class. Using the Barton Nackman trick.
 * 
 **/

  T& asLeaf() { return static_cast<T&>(*this);}  

  bool initStencils() {
    return asLeaf().initStencils();
  }

  bool preAddVec() {
    return asLeaf().preAddVec();
  }

  bool postAddVec() {
    return asLeaf().postAddVec();
  }

  bool preComputeVec() {
    return asLeaf().preComputeVec();
  }

  bool postComputeVec() {
    return asLeaf().postComputeVec();
  }

  void setDof(unsigned int dof) { m_uiDof = dof; }
  unsigned int getDof() { return m_uiDof; }

  void setTimeInfo(timeInfo *t) { m_time =t; }
  timeInfo* getTimeInfo() { return m_time; }

  void initOctLut();

	inline int getEtype(unsigned char hnMask, unsigned char cNum) {
		unsigned char type=0;
		if(hnMask) {
	    switch(cNum) {
	      case 0:
					type = ot::getElemType<0>(hnMask);
					break;
	      case 1:
					type = ot::getElemType<1>(hnMask);
					break;
	      case 2:
					type = ot::getElemType<2>(hnMask);
					break;
	      case 3:
					type = ot::getElemType<3>(hnMask);
					break;
	      case 4:
					type = ot::getElemType<4>(hnMask);
					break;
	      case 5:
					type = ot::getElemType<5>(hnMask);
					break;
	      case 6:
					type = ot::getElemType<6>(hnMask);
					break;
	      case 7:
					type = ot::getElemType<7>(hnMask);
					break;
	      default:
					assert(false);
	    }
		}
		return type;
  }

  inline PetscErrorCode alignElementAndVertices(ot::DA * da, stdElemType & sType, unsigned int* indices);
  inline PetscErrorCode mapVtxAndFlagsToOrientation(int childNum, unsigned int* indices, unsigned char & mask);
  inline PetscErrorCode reOrderIndices(unsigned char eType, unsigned int* indices);

protected:
  void *          	m_stencil;

  std::string     	m_strVectorType;

  timeInfo		*m_time;

  unsigned int		m_uiDof;

  // Octree specific stuff ...
  unsigned char **	m_ucpLut;
};

template <typename T>
feVector<T>::feVector() {
  m_daType = PETSC;
  m_DA 		= NULL;
  m_octDA 	= NULL;
  m_stencil	= NULL;
  m_uiDof	= 1;
  m_ucpLut	= NULL;

  // initialize the stencils ...
  initStencils();
}

template <typename T>
feVector<T>::feVector(daType da) {
#ifdef __DEBUG__
  assert ( ( da == PETSC ) || ( da == OCT ) );
#endif
  m_daType = da;
  m_DA 		= NULL;
  m_octDA 	= NULL;
  m_stencil	= NULL;
  m_ucpLut	= NULL;

  // initialize the stencils ...
  initStencils();
  if (da == OCT)
    initOctLut();
}

template <typename T>
void feVector<T>::initOctLut() {
  //Note: It is not symmetric.
  unsigned char tmp[8][8]={
    {0,1,2,3,4,5,6,7},
    {1,3,0,2,5,7,4,6},
    {2,0,3,1,6,4,7,5},
    {3,2,1,0,7,6,5,4},
    {4,5,0,1,6,7,2,3},
    {5,7,1,3,4,6,0,2},
    {6,4,2,0,7,5,3,1},
    {7,6,3,2,5,4,1,0}
  };

  //Is Stored in  ROW_MAJOR Format.  
  typedef unsigned char* charPtr;
  m_ucpLut = new charPtr[8];
  for (int i=0;i<8;i++) {
    m_ucpLut[i] = new unsigned char[8]; 
    for (int j=0;j<8;j++) {
      m_ucpLut[i][j] = tmp[i][j];
    }
  }
}

template <typename T>
feVector<T>::~feVector() {
}


/**
 * 	@brief		The matrix-vector multiplication routine that is used by
 * 				matrix-free methods. 
 * 	@param		_in	PETSc Vec which is the input vector with whom the 
 * 				product is to be calculated.
 * 	@param		_out PETSc Vec, the output of M*_in
 * 	@return		bool true if successful, false otherwise.
 * 
 *  The matrix-vector multiplication routine that is used by matrix-free 
 * 	methods. The product is directly calculated from the elemental matrices,
 *  which are computed by the ElementalMatrix() function. Use the Assemble()
 *  function for matrix based methods.
 **/
/*#undef __FUNCT__
  #define __FUNCT__ "feVector_AddVec"
  template <typename T>
  bool feVector<T>::addVec(Vec _in, double scale){
  PetscFunctionBegin;

  #ifdef __DEBUG__
  assert ( ( m_daType == PETSC ) || ( m_daType == OCT ) );
  #endif

  int ierr;
  // PetscScalar zero=0.0;

  if (m_daType == PETSC) {

  PetscInt x,y,z,m,n,p;
  PetscInt mx,my,mz;
  int xne,yne,zne;

  PetscScalar ***in;
  Vec inlocal;

  // Get all corners
  if (m_DA == NULL)
  std::cerr << "Da is null" << std::endl;
  ierr = DAGetCorners(m_DA, &x, &y, &z, &m, &n, &p); CHKERRQ(ierr); 
  // Get Info
  ierr = DAGetInfo(m_DA,0, &mx, &my, &mz, 0,0,0,0,0,0,0); CHKERRQ(ierr); 

  if (x+m == mx) {
  xne=m-1;
  } else {
  xne=m;
  }
  if (y+n == my) {
  yne=n-1;
  } else {
  yne=n;
  }
  if (z+p == mz) {
  zne=p-1;
  } else {
  zne=p;
  }

  double norm;

  #ifdef __DEBUG__
  VecNorm(_in, NORM_INFINITY, &norm);
  std::cout << " norm of _in in feVector.cpp before adding force = " << norm << std::endl;
  #endif
	 
  ierr = DAGetLocalVector(m_DA,&inlocal); CHKERRQ(ierr);

  ierr = VecZeroEntries(inlocal); CHKERRQ(ierr);

  ierr = DAVecGetArray(m_DA,inlocal, &in);

  // Any derived class initializations ...
  preAddVec();

  // loop through all elements ...
  for (int k=z; k<z+zne; k++){
  for (int j=y; j<y+yne; j++){
  for (int i=x; i<x+xne; i++){
  // std::cout << i <<"," << j << "," << k << std::endl;
  ElementalAddVec(i, j, k, in, scale);
  } // end i
  } // end j
  } // end k

  postAddVec();

  ierr = DAVecRestoreArray(m_DA, inlocal, &in);

  ierr = DALocalToGlobalBegin(m_DA,inlocal,_in); CHKERRQ(ierr);
  ierr = DALocalToGlobalEnd(m_DA,inlocal,_in); CHKERRQ(ierr);

  ierr = DARestoreLocalVector(m_DA,&inlocal); CHKERRQ(ierr);

  #ifdef __DEBUG__
  VecNorm(_in, NORM_INFINITY, &norm);
  std::cout << " norm of _in in feVector.cpp after adding force = " << norm << std::endl;
  #endif
  // ierr = VecDestroy(outlocal); CHKERRQ(ierr);  

  } else {
  // loop for octree DA.
    

  PetscScalar *out=NULL;
  PetscScalar *in=NULL; 

  // get Buffers ...
  //Nodal,Non-Ghosted,Read,1 dof, Get in array and get ghosts during computation
  m_octDA->vecGetBuffer(_in,   in, false, false, false,  m_uiDof);

    
  // start comm for in ...
  //m_octDA->updateGhostsBegin<PetscScalar>(in, false, m_uiDof);
  m_octDA->ReadFromGhostsBegin<PetscScalar>(in, false, m_uiDof);
  preAddVec();

  // Independent loop, loop through the nodes this processor owns..
  for ( m_octDA->init<ot::DA::INDEPENDENT>(), m_octDA->init<ot::DA::WRITABLE>(); m_octDA->curr() < m_octDA->end<ot::DA::INDEPENDENT>(); m_octDA->next<ot::DA::INDEPENDENT>() ) {
  ElementalAddVec( m_octDA->curr(), in, scale); 
  }//end INDEPENDENT

  // Wait for communication to end.
  //m_octDA->updateGhostsEnd<PetscScalar>(in);
  m_octDA->ReadFromGhostsEnd<PetscScalar>(in);
	 
  // Dependent loop ...
  for ( m_octDA->init<ot::DA::DEPENDENT>(), m_octDA->init<ot::DA::WRITABLE>();m_octDA->curr() < m_octDA->end<ot::DA::DEPENDENT>(); m_octDA->next<ot::DA::DEPENDENT>() ) {
  ElementalAddVec( m_octDA->curr(), in, scale); 
  }//end DEPENDENT

  postAddVec();

  // Restore Vectors ...
  m_octDA->vecRestoreBuffer(_in,   in, false, false, false,  m_uiDof);

  }

  PetscFunctionReturn(0);
  }
*/

/**
 * 	@brief		The matrix-vector multiplication routine that is used by
 * 				matrix-free methods. 
 * 	@param		_in	PETSc Vec which is the input vector with whom the 
 * 				product is to be calculated.
 * 	@param		_out PETSc Vec, the output of M*_in
 * 	@return		bool true if successful, false otherwise.
 * 
 *  The matrix-vector multiplication routine that is used by matrix-free 
 * 	methods. The product is directly calculated from the elemental matrices,
 *  which are computed by the ElementalMatrix() function. Use the Assemble()
 *  function for matrix based methods.
 **/
#undef __FUNCT__
#define __FUNCT__ "feVector_AddVec_Indx"
template <typename T>
bool feVector<T>::addVec(Vec _in, double scale, int indx){
  PetscFunctionBegin;

#ifdef __DEBUG__
  assert ( ( m_daType == PETSC ) || ( m_daType == OCT ) );
#endif

  int ierr;
  // PetscScalar zero=0.0;

  m_iCurrentDynamicIndex = indx;
  
  if (m_daType == PETSC) {

    PetscInt x,y,z,m,n,p;
    PetscInt mx,my,mz;
    int xne,yne,zne;

    PetscScalar ***in;
    Vec inlocal;

    /* Get all corners*/
    if (m_DA == NULL)
      std::cerr << "Da is null" << std::endl;
    ierr = DMDAGetCorners(m_DA, &x, &y, &z, &m, &n, &p); CHKERRQ(ierr); 
    /* Get Info*/
    ierr = DMDAGetInfo(m_DA,0, &mx, &my, &mz, 0,0,0,0,0,0,0,0,0); CHKERRQ(ierr); 

    if (x+m == mx) {
      xne=m-1;
    } else {
      xne=m;
    }
    if (y+n == my) {
      yne=n-1;
    } else {
      yne=n;
    }
    if (z+p == mz) {
      zne=p-1;
    } else {
      zne=p;
    }

	 // double norm;

#ifdef __DEBUG__
	 VecNorm(_in, NORM_INFINITY, &norm);
	 std::cout << " norm of _in in feVector.cpp before adding force = " << norm << std::endl;
#endif
	 
	 ierr = DMGetLocalVector(m_DA,&inlocal); CHKERRQ(ierr);

	 ierr = VecZeroEntries(inlocal); CHKERRQ(ierr);

	 ierr = DMDAVecGetArray(m_DA,inlocal, &in);

    // Any derived class initializations ...
   // std::cout << __func__ << " -> preAddVec " << std::endl; 
   preAddVec();

   // std::cout << __func__ << " -> Elemental Loop " << std::endl;
    // loop through all elements ...
    for (int k=z; k<z+zne; k++){
      for (int j=y; j<y+yne; j++){
        for (int i=x; i<x+xne; i++){
          // std::cout << i <<"," << j << "," << k << std::endl;
          ElementalAddVec(i, j, k, in, scale);
        } // end i
      } // end j
    } // end k

    // std::cout << __func__ << " -> postAddVec " << std::endl;
    postAddVec();

	 ierr = DMDAVecRestoreArray(m_DA, inlocal, &in);

	 ierr = DMLocalToGlobalBegin(m_DA,inlocal,ADD_VALUES,_in); CHKERRQ(ierr);
	 ierr = DMLocalToGlobalEnd(m_DA,inlocal,ADD_VALUES,_in); CHKERRQ(ierr);

	 ierr = DMRestoreLocalVector(m_DA,&inlocal); CHKERRQ(ierr);

#ifdef __DEBUG__
	 VecNorm(_in, NORM_INFINITY, &norm);
	 std::cout << " norm of _in in feVector.cpp after adding force = " << norm << std::endl;
#endif
    // ierr = VecDestroy(outlocal); CHKERRQ(ierr);  

  } else {
    // loop for octree DA.
    

    // PetscScalar *out=NULL;
    PetscScalar *in=NULL; 

    // get Buffers ...
    //Nodal,Non-Ghosted,Read,1 dof, Get in array and get ghosts during computation
    m_octDA->vecGetBuffer(_in,   in, false, false, false,  m_uiDof);

    
    // start comm for in ...
    //m_octDA->updateGhostsBegin<PetscScalar>(in, false, m_uiDof);
    //m_octDA->ReadFromGhostsBegin<PetscScalar>(in, false, m_uiDof);
    m_octDA->ReadFromGhostsBegin<PetscScalar>(in, m_uiDof);
    preAddVec();

    // Independent loop, loop through the nodes this processor owns..
    for ( m_octDA->init<ot::DA_FLAGS::INDEPENDENT>(), m_octDA->init<ot::DA_FLAGS::WRITABLE>(); m_octDA->curr() < m_octDA->end<ot::DA_FLAGS::INDEPENDENT>(); m_octDA->next<ot::DA_FLAGS::INDEPENDENT>() ) {
      ElementalAddVec( m_octDA->curr(), in, scale); 
    }//end INDEPENDENT

    // Wait for communication to end.
    //m_octDA->updateGhostsEnd<PetscScalar>(in);
	 m_octDA->ReadFromGhostsEnd<PetscScalar>(in);
	 
    // Dependent loop ...
    for ( m_octDA->init<ot::DA_FLAGS::DEPENDENT>(), m_octDA->init<ot::DA_FLAGS::WRITABLE>();m_octDA->curr() < m_octDA->end<ot::DA_FLAGS::DEPENDENT>(); m_octDA->next<ot::DA_FLAGS::DEPENDENT>() ) {
      ElementalAddVec( m_octDA->curr(), in, scale); 
    }//end DEPENDENT

    postAddVec();

    // Restore Vectors ...
    m_octDA->vecRestoreBuffer(_in,   in, false, false, false,  m_uiDof);

  }

  PetscFunctionReturn(0);
}

/**
 * 	@brief		The nonlinear reaction term used in operator splitting or any node related functions
 * 	@param		_in	PETSc Vec which is the input vector with whom the 
 * 				product is to be calculated.
 * 	@param		_out PETSc Vec, the output of Reaction(in)
 * 	@return		bool true if successful, false otherwise.
 * 
 *  The nonlinear reaction term used in operator splitting scheme.
 *  The product is directly calculated from the elemental matrices,
 *  ComputeNodalFunction
 **/
#undef __FUNCT__
#define __FUNCT__ "feVector_ComputeVec"
template <typename T>
bool feVector<T>::computeVec(Vec _in, Vec _out,double scale){
  PetscFunctionBegin;

#ifdef __DEBUG__
  assert ( ( m_daType == PETSC ) || ( m_daType == OCT ) );
#endif

  int ierr;
  // PetscScalar zero=0.0;

  if (m_daType == PETSC) {

    PetscInt x,y,z,m,n,p;
    // PetscInt mx,my,mz;
    // int xne,yne,zne;

    PetscScalar ***in;
	 PetscScalar ***out;

	 
    /* Get all corners*/
    if (m_DA == NULL)
      std::cerr << "Da is null" << std::endl;

	 ierr = DMDAGetCorners(m_DA,&x,&y,&z,&m,&n,&p); CHKERRQ(ierr);
	 ierr = DMDAVecGetArray(m_DA,_in,&in); CHKERRQ(ierr);
	 ierr = DMDAVecGetArray(m_DA,_out,&out); CHKERRQ(ierr);
    // Any derived class initializations ...
    preComputeVec();

    // loop through all elements ...  // PROBABLY NODES
    for (int k=z; k<z+p; k++){
      for (int j=y; j<y+n; j++){
        for (int i=x; i<x+m; i++){
          // std::cout << i <<"," << j << "," << k << std::endl;
          ComputeNodalFunction(i, j, k, in, out,scale);
        } // end i
      } // end j
    } // end k

    postComputeVec();


	 ierr = DMDAVecRestoreArray(m_DA,_in,&in); CHKERRQ(ierr);
	 ierr = DMDAVecRestoreArray(m_DA,_out,&out); CHKERRQ(ierr);

  } else {
    // loop for octree DA.
    

    PetscScalar *out=NULL;
    PetscScalar *in=NULL; 

    // get Buffers ...
    //Nodal,Non-Ghosted,Read,1 dof, Get in array 
    m_octDA->vecGetBuffer(_in,   in, false, false, true,  m_uiDof);
    m_octDA->vecGetBuffer(_in,   out, false, false, false,m_uiDof);

    preAddVec();

    // Independent loop, loop through the nodes this processor owns..
    for ( m_octDA->init<ot::DA_FLAGS::INDEPENDENT>(), m_octDA->init<ot::DA_FLAGS::WRITABLE>(); m_octDA->curr() < m_octDA->end<ot::DA_FLAGS::INDEPENDENT>(); m_octDA->next<ot::DA_FLAGS::INDEPENDENT>() ) {
     ComputeNodalFunction(in, out,scale); 
    }//end INDEPENDENT


    postAddVec();

    // Restore Vectors ...
    m_octDA->vecRestoreBuffer(_in,   in, false, false, true,  m_uiDof);
	 m_octDA->vecRestoreBuffer(_out,  out,false, false, false, m_uiDof);

  }

  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "alignElementAndVertices"
template <typename T>
PetscErrorCode feVector<T>::alignElementAndVertices(ot::DA * da, stdElemType & sType, unsigned int* indices) {
  PetscFunctionBegin;
  
  sType = ST_0;
  da->getNodeIndices(indices); 

  // not required ....
  // int rank;
  // MPI_Comm_rank(da->getComm(), &rank);

  if (da->isHanging(da->curr())) {

    int childNum = da->getChildNumber();
    Point pt = da->getCurrentOffset();   

    unsigned char hangingMask = da->getHangingNodeIndex(da->curr());    

    //Change HangingMask and indices based on childNum
    mapVtxAndFlagsToOrientation(childNum, indices, hangingMask);    

    unsigned char eType = ((126 & hangingMask)>>1);

    reOrderIndices(eType, indices);
  }//end if hangingElem.
  PetscFunctionReturn(0);
}//end function.

#undef __FUNCT__
#define __FUNCT__ "mapVtxAndFlagsToOrientation"
template <typename T>
PetscErrorCode feVector<T>::mapVtxAndFlagsToOrientation(int childNum, unsigned int* indices, unsigned char & mask) {
  PetscFunctionBegin;
  unsigned int tmp[8];
  unsigned char tmpFlags = 0;
  for (int i=0;i<8;i++) {
    tmp[i] = indices[m_ucpLut[childNum][i]];
    tmpFlags = ( tmpFlags | ( ( (1<<(m_ucpLut[childNum][i])) & mask ) ? (1<<i) : 0 ) );
  }
  for (int i=0;i<8;i++) {
    indices[i] = tmp[i];
  }
  mask = tmpFlags;
  PetscFunctionReturn(0);
}//end function

#undef __FUNCT__
#define __FUNCT__ "reOrderIndices"
template <typename T>
PetscErrorCode feVector<T>::reOrderIndices(unsigned char eType, unsigned int* indices) {
#ifdef __DEBUG_1
  std::cout << "Entering " << __func__ << std::endl;
#endif
  PetscFunctionBegin;
  unsigned int tmp;
  switch (eType) {
  case  ET_N: 
    break;
  case  ET_Y:
    break;
  case  ET_X:
    //Swap 1 & 2, Swap 5 & 6
    tmp = indices[1];
    indices[1] = indices[2];
    indices[2] = tmp;
    tmp = indices[5];
    indices[5] = indices[6];
    indices[6] = tmp;
    break;
  case  ET_XY:
    break;
  case  ET_Z:
    //Swap 2 & 4, Swap 3 & 5
    tmp = indices[2];
    indices[2] = indices[4];
    indices[4] = tmp;
    tmp = indices[3];
    indices[3] = indices[5];
    indices[5] = tmp;
    break;
  case  ET_ZY:
    //Swap 1 & 4, Swap 3 & 6
    tmp = indices[1];
    indices[1] = indices[4];
    indices[4] = tmp;
    tmp = indices[3];
    indices[3] = indices[6];
    indices[6] = tmp;
    break;
  case  ET_ZX:
    //Swap 2 & 4, Swap 3 & 5
    tmp = indices[2];
    indices[2] = indices[4];
    indices[4] = tmp;
    tmp = indices[3];
    indices[3] = indices[5];
    indices[5] = tmp;
    break;
  case  ET_ZXY:
    break;
  case  ET_XY_XY:
    break;
  case  ET_XY_ZXY:
    break;
  case  ET_YZ_ZY:
    //Swap 1 & 4, Swap 3 & 6
    tmp = indices[1];
    indices[1] = indices[4];
    indices[4] = tmp;
    tmp = indices[3];
    indices[3] = indices[6];
    indices[6] = tmp;
    break;
  case  ET_YZ_ZXY:
    //Swap 1 & 4, Swap 3 & 6
    tmp = indices[1];
    indices[1] = indices[4];
    indices[4] = tmp;
    tmp = indices[3];
    indices[3] = indices[6];
    indices[6] = tmp;
    break;
  case  ET_YZ_XY_ZXY:
    break;
  case  ET_ZX_ZX:
    //Swap 2 & 4, Swap 3 & 5
    tmp = indices[2];
    indices[2] = indices[4];
    indices[4] = tmp;
    tmp = indices[3];
    indices[3] = indices[5];
    indices[5] = tmp;
    break;
  case  ET_ZX_ZXY:
    //Swap 2 & 4, Swap 3 & 5
    tmp = indices[2];
    indices[2] = indices[4];
    indices[4] = tmp;
    tmp = indices[3];
    indices[3] = indices[5];
    indices[5] = tmp;
    break;
  case  ET_ZX_XY_ZXY:
    //Swap 1 & 2, Swap 5 & 6
    tmp = indices[1];
    indices[1] = indices[2];
    indices[2] = tmp;
    tmp = indices[5];
    indices[5] = indices[6];
    indices[6] = tmp;
    break;
  case  ET_ZX_YZ_ZXY:
    //Swap 2 & 4, Swap 3 & 5
    tmp = indices[2];
    indices[2] = indices[4];
    indices[4] = tmp;
    tmp = indices[3];
    indices[3] = indices[5];
    indices[5] = tmp;
    break;
  case  ET_ZX_YZ_XY_ZXY:
    break;
  default:
    std::cout<<"in reOrder Etype: "<< (int) eType << std::endl;
    assert(false);
  }
#ifdef __DEBUG_1
  std::cout << "Leaving " << __func__ << std::endl;
#endif
  PetscFunctionReturn(0);
}


#endif
