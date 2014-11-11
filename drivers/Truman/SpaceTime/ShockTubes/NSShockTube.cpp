//  Camellia
//
//  Created by Truman Ellis on 6/4/2012.

#include "InnerProductScratchPad.h"
#include "RefinementStrategy.h"
#include "SolutionExporter.h"
#include "PreviousSolutionFunction.h"

#ifdef HAVE_MPI
#include <Teuchos_GlobalMPISession.hpp>
#include "mpi_choice.hpp"
#else
#include "choice.hpp"
#endif

double pi = 2.0*acos(0.0);

int H1Order = 3, pToAdd = 2;

class ConstantXBoundary : public SpatialFilter {
   private:
      double xval;
   public:
      ConstantXBoundary(double xval): xval(xval) {};
      bool matchesPoint(double x, double y) {
         double tol = 1e-14;
         return (abs(x-xval) < tol);
      }
};

class ConstantYBoundary : public SpatialFilter {
   private:
      double yval;
   public:
      ConstantYBoundary(double yval): yval(yval) {};
      bool matchesPoint(double x, double y) {
         double tol = 1e-14;
         return (abs(y-yval) < tol);
      }
};

class PowerFunction : public Function {
  private:
    FunctionPtr _function;
    double _power;
  public:
    PowerFunction(FunctionPtr function, double power) : Function(0), _function(function), _power(power) {}
    void values(FieldContainer<double> &values, BasisCachePtr basisCache) {
      int numCells = values.dimension(0);
      int numPoints = values.dimension(1);

      _function->values(values, basisCache);

      const FieldContainer<double> *points = &(basisCache->getPhysicalCubaturePoints());
      for (int cellIndex=0; cellIndex<numCells; cellIndex++) {
        for (int ptIndex=0; ptIndex<numPoints; ptIndex++) {
          values(cellIndex, ptIndex) = pow(values(cellIndex, ptIndex), _power);
        }
      }
    }
};

class ExpFunction : public Function {
  private:
    FunctionPtr _function;
  public:
    ExpFunction(FunctionPtr function) : Function(0), _function(function) {}
    void values(FieldContainer<double> &values, BasisCachePtr basisCache) {
      int numCells = values.dimension(0);
      int numPoints = values.dimension(1);

      _function->values(values, basisCache);

      const FieldContainer<double> *points = &(basisCache->getPhysicalCubaturePoints());
      for (int cellIndex=0; cellIndex<numCells; cellIndex++) {
        for (int ptIndex=0; ptIndex<numPoints; ptIndex++) {
          values(cellIndex, ptIndex) = exp(values(cellIndex, ptIndex));
        }
      }
    }
};

class SqrtFunction : public Function {
  private:
    FunctionPtr _function;
  public:
    SqrtFunction(FunctionPtr function) : Function(0), _function(function) {}
    void values(FieldContainer<double> &values, BasisCachePtr basisCache) {
      int numCells = values.dimension(0);
      int numPoints = values.dimension(1);

      _function->values(values, basisCache);

      const FieldContainer<double> *points = &(basisCache->getPhysicalCubaturePoints());
      for (int cellIndex=0; cellIndex<numCells; cellIndex++) {
        for (int ptIndex=0; ptIndex<numPoints; ptIndex++) {
          values(cellIndex, ptIndex) = sqrt(values(cellIndex, ptIndex));
        }
      }
    }
};

class BoundedBelowFunction : public Function {
  private:
    FunctionPtr _function;
    double _bound;
  public:
    BoundedBelowFunction(FunctionPtr function, double bound) : Function(0), _function(function), _bound(bound) {}
    void values(FieldContainer<double> &values, BasisCachePtr basisCache) {
      int numCells = values.dimension(0);
      int numPoints = values.dimension(1);

      _function->values(values, basisCache);

      const FieldContainer<double> *points = &(basisCache->getPhysicalCubaturePoints());
      for (int cellIndex=0; cellIndex<numCells; cellIndex++) {
        for (int ptIndex=0; ptIndex<numPoints; ptIndex++) {
          values(cellIndex, ptIndex) = max(values(cellIndex, ptIndex),_bound);
        }
      }
    }
};

class BoundedSqrtFunction : public Function {
  private:
    FunctionPtr _function;
    double _bound;
  public:
    BoundedSqrtFunction(FunctionPtr function, double bound) : Function(0), _function(function), _bound(bound) {}
    void values(FieldContainer<double> &values, BasisCachePtr basisCache) {
      int numCells = values.dimension(0);
      int numPoints = values.dimension(1);

      _function->values(values, basisCache);

      const FieldContainer<double> *points = &(basisCache->getPhysicalCubaturePoints());
      for (int cellIndex=0; cellIndex<numCells; cellIndex++) {
        for (int ptIndex=0; ptIndex<numPoints; ptIndex++) {
          values(cellIndex, ptIndex) = sqrt(max(values(cellIndex, ptIndex),_bound));
        }
      }
    }
};

class DivergenceIndicator : public Function {
  private:
    FunctionPtr _function;
  public:
    DivergenceIndicator(FunctionPtr function) : Function(0), _function(function) {}
    void values(FieldContainer<double> &values, BasisCachePtr basisCache) {
      int numCells = values.dimension(0);
      int numPoints = values.dimension(1);

      _function->values(values, basisCache);

      const FieldContainer<double> *points = &(basisCache->getPhysicalCubaturePoints());
      for (int cellIndex=0; cellIndex<numCells; cellIndex++) {
        for (int ptIndex=0; ptIndex<numPoints; ptIndex++) {
          if (values(cellIndex,ptIndex) < -1e-3)
            values(cellIndex, ptIndex) = 1./(-values(cellIndex, ptIndex));
            // values(cellIndex, ptIndex) = 1;
          else
            values(cellIndex, ptIndex) = 0;
        }
      }
    }
};

class ErrorFunction : public Function {
  private:
    SolutionPtr _solution;
  public:
    ErrorFunction(SolutionPtr solution) : Function(0), _solution(solution) {}
    void values(FieldContainer<double> &values, BasisCachePtr basisCache) {
      int numCells = values.dimension(0);
      int numPoints = values.dimension(1);

      // _function->values(values, basisCache);
      map<int, double> errorMap = _solution->energyError();

      const FieldContainer<double> *points = &(basisCache->getPhysicalCubaturePoints());
      for (int cellIndex=0; cellIndex<numCells; cellIndex++) {
        map<int, double>::iterator it = errorMap.find(cellIndex);
        if (it != errorMap.end())
          cout << "cell " << cellIndex << " error " << errorMap[cellIndex] << endl;
        else
          cout << " Problem " << endl;
        for (int ptIndex=0; ptIndex<numPoints; ptIndex++) {
          // values(cellIndex, ptIndex) = errorMap[cellIndex];
          values(cellIndex, ptIndex) = 0;
        }
      }
    }
};

class PulseInitialCondition : public Function {
   private:
      double width;
      double valI;
      double valO;
   public:
      PulseInitialCondition(double width, double valI, double valO) : Function(0), width(width), valI(valI), valO(valO) {}
      void values(FieldContainer<double> &values, BasisCachePtr basisCache) {
         int numCells = values.dimension(0);
         int numPoints = values.dimension(1);

         const FieldContainer<double> *points = &(basisCache->getPhysicalCubaturePoints());
         for (int cellIndex=0; cellIndex<numCells; cellIndex++) {
            for (int ptIndex=0; ptIndex<numPoints; ptIndex++) {
               double x = (*points)(cellIndex,ptIndex,0);
               if (abs(x) <= width/2.)
                  values(cellIndex, ptIndex) = valI;
               else
                  values(cellIndex, ptIndex) = valO;
            }
         }
      }
};

class DiscontinuousInitialCondition : public Function {
   private:
      double xloc;
      double valL;
      double valR;
   public:
      DiscontinuousInitialCondition(double xloc, double valL, double valR) : Function(0), xloc(xloc), valL(valL), valR(valR) {}
      void values(FieldContainer<double> &values, BasisCachePtr basisCache) {
         int numCells = values.dimension(0);
         int numPoints = values.dimension(1);

         const FieldContainer<double> *points = &(basisCache->getPhysicalCubaturePoints());
         for (int cellIndex=0; cellIndex<numCells; cellIndex++) {
            for (int ptIndex=0; ptIndex<numPoints; ptIndex++) {
               double x = (*points)(cellIndex,ptIndex,0);
               if (x <= xloc)
                  values(cellIndex, ptIndex) = valL;
               else
                  values(cellIndex, ptIndex) = valR;
            }
         }
      }
};

class RampedInitialCondition : public Function {
   private:
      double xloc;
      double valL;
      double valR;
      double h;
   public:
      RampedInitialCondition(double xloc, double valL, double valR, double h) : Function(0), xloc(xloc), valL(valL), valR(valR), h(h) {}
      void setH(double h_) { h = h_; }
      void values(FieldContainer<double> &values, BasisCachePtr basisCache) {
         int numCells = values.dimension(0);
         int numPoints = values.dimension(1);

         const FieldContainer<double> *points = &(basisCache->getPhysicalCubaturePoints());
         for (int cellIndex=0; cellIndex<numCells; cellIndex++) {
            for (int ptIndex=0; ptIndex<numPoints; ptIndex++) 
            {
               double x = (*points)(cellIndex,ptIndex,0);
               if (x <= xloc-h/2.)
                  values(cellIndex, ptIndex) = valL;
               else if (x >= xloc+h/2.)
                  values(cellIndex, ptIndex) = valR;
               else
                  values(cellIndex, ptIndex) = valL+(valR-valL)/h*(x-xloc+h/2);
            }
         }
      }
};

int main(int argc, char *argv[]) {
#ifdef HAVE_MPI
  Teuchos::GlobalMPISession mpiSession(&argc, &argv,0);
  choice::MpiArgs args( argc, argv );
#else
  choice::Args args( argc, argv );
#endif
  int commRank = Teuchos::GlobalMPISession::getRank();
  int numProcs = Teuchos::GlobalMPISession::getNProc();

   // Required arguments
  int problem = args.Input<int>("--problem", "which problem to run");
  int formulation = args.Input<int>("--formulation", "which formulation to use: 0 = primitive, 1 = conservation, 2 = entropy");

   // Optional arguments (have defaults)
  int numRefs = args.Input("--numRefs", "number of refinement steps", 0);
  int norm = args.Input("--norm", "norm", 0);
  double physicalViscosity = args.Input("--mu", "viscosity", 1e-2);
  double mu = physicalViscosity;
  double mu_sqrt = sqrt(mu);
  int numSlabs = args.Input("--numSlabs", "number of time slabs", 1);
  bool useLineSearch = args.Input("--lineSearch", "use line search", true);
  int polyOrder = args.Input("--polyOrder", "polynomial order for field variables", 2);
  int deltaP = args.Input("--deltaP", "how much to enrich test space", 2);
  int numX = args.Input("--numX", "number of cells in the x direction", 4);
  int numT = args.Input("--numT", "number of cells in the t direction", 1);
  int maxNewtonIterations = args.Input("--maxIterations", "maximum number of Newton iterations", 10);
  double nlTol = args.Input("--nlTol", "nonlinear tolerance", 1e-6);

  args.Process();

   ////////////////////   PROBLEM DEFINITIONS   ///////////////////////
  int H1Order = polyOrder+1;

  double xmin, xmax, xint, tmin, tmax;
  double rhoL, rhoR, uL, uR, pL, pR, eL, eR, TL, TR, UcL, UcR, UmL, UmR, UeL, UeR, mL, mR, EL, ER;
  double Pr = 0.713;
  double gamma = 1.4;
  double p0 = 1;
  double rho0 = 1;
  double u0 = 1;
  double a0 = sqrt(gamma*p0/rho0);
  double M_inf = u0/a0;
  double Cv = 1./(gamma*(gamma-1)*M_inf*M_inf);
  double Cp = gamma*Cv;
  double R = Cp-Cv;
  string problemName;

  switch (problem)
  {
    case 0:
    // Trivial problem for testing
    problemName = "Trivial";
    xmin = 0;
    xmax = 1;
    xint = 0.5;
    tmax = 0.1;

    rhoL = 1;
    rhoR = 1;
    uL = 1;
    uR = 1;
    TL = 1;
    TR = 1;
    break;
    case 1:
    // Simple shock for testing
    problemName = "SimpleShock";
    xmin = 0;
    xmax = 1;
    xint = 0.5;
    tmax = 0.1;

    rhoL = 1;
    rhoR = 1;
    uL = 1;
    uR = 0;
    TL = 1;
    TR = 1;
    break;
    case 2:
    // Simple rarefaction for testing
    problemName = "SimpleRarefaction";
    xmin = 0;
    xmax = 1;
    xint = 0.5;
    tmax = 0.1;

    rhoL = 1;
    rhoR = 1;
    uL = -1;
    uR = 1;
    TL = 1;
    TR = 1;
    break;
    case 3:
    // Sod shock tube
    problemName = "Sod";
    xmin = 0;
    xmax = 1;
    xint = 0.5;
    tmax = 0.2;

    rhoL = 1;
    rhoR = .125;
    uL = 0;
    uR = 0;
    TL = 1/(rhoL*R);
    TR = .1/(rhoR*R);
    break;
    case 4:
    // Strong shock tube
    problemName = "StrongShock";
    xmin = 0;
    xmax = 5;
    xint = 2.5;
    tmax = 4e-1;

    rhoL = 10;
    rhoR = 1;
    uL = 0;
    uR = 0;
    TL = 100/(rhoL*R);
    TR = 1/(rhoR*R);
    break;
    case 5:
    // Strong shock tube
    problemName = "Noh";
    gamma = 5./3;
    a0 = sqrt(gamma*p0/rho0);
    M_inf = u0/a0;
    Cv = 1./(gamma*(gamma-1)*M_inf*M_inf);
    Cp = gamma*Cv;
    R = Cp-Cv;
    xmin = 0;
    xmax = 1;
    xint = .5;
    tmax = .25;

    rhoL = 1;
    rhoR = 1;
    uL = 1;
    uR = -1;
    // TL = p0/(rho0*R*T0);
    // TR = p0/(rho0*R*T0);
    TL = 0;
    TR = 0;
    break;
    case 6:
    // Strong shock tube
    problemName = "Sedov";
    xmin = -.5;
    xmax = 0.5;
    xint = 0;
    tmax = 0.1;

    rhoL = 1;
    rhoR = 1;
    uL = 0;
    uR = 0;
    TL = 1;
    TR = 1;
    break;
    default:
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "invalid problem number");
  }
  if (commRank == 0)
    cout << "Running the " << problemName << " problem" << endl;

  switch (formulation)
  {
    case 0:
    UcL = rhoL;
    UcR = rhoR;
    UmL = uL;
    UmR = uR;
    UeL = TL;
    UeR = TR;
    break;
    case 1:
    UcL = rhoL;
    UcR = rhoR;
    UmL = rhoL*uL;
    UmR = rhoL*uR;
    UeL = rhoL*(Cv*TL+0.5*uL*uL);
    UeR = rhoR*(Cv*TR+0.5*uR*uR);
    break;
    case 2:
    mL = rhoL*uL;
    mR = rhoL*uR;
    EL = rhoL*(Cv*TL+0.5*uL*uL);
    ER = rhoR*(Cv*TR+0.5*uR*uR);
    UcL = (-EL+(EL-0.5*mL*mL/rhoL)*(gamma+1-log(((gamma-1)*(EL-0.5*mL*mL/rhoL)/pow(rhoL,gamma)))))/(EL-0.5*mL*mL/rhoL);
    UcR = (-ER+(ER-0.5*mR*mR/rhoR)*(gamma+1-log(((gamma-1)*(ER-0.5*mR*mR/rhoR)/pow(rhoR,gamma)))))/(ER-0.5*mR*mR/rhoR);
    UmL = mL/(EL-0.5*mL*mL/rhoL);
    UmR = mR/(ER-0.5*mR*mR/rhoR);
    UeL = -rhoL/(EL-0.5*mL*mL/rhoL);
    UeR = -rhoR/(ER-0.5*mR*mR/rhoR);
    break;
    default:
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "invalid problem number");
  }

  ////////////////////   DECLARE VARIABLES   ///////////////////////
  // define test variables
  VarFactory varFactory;
  VarPtr S = varFactory.testVar("S", HGRAD);
  VarPtr tau = varFactory.testVar("tau", HGRAD);
  VarPtr vc = varFactory.testVar("vc", HGRAD);
  VarPtr vm = varFactory.testVar("vm", HGRAD);
  VarPtr ve = varFactory.testVar("ve", HGRAD);

  // define trial variables
  // common variables amongst formulations
  VarPtr D = varFactory.fieldVar("D");
  VarPtr q = varFactory.fieldVar("q");
  VarPtr uhat = varFactory.spatialTraceVar("uhat");
  VarPtr That = varFactory.spatialTraceVar("That");
  VarPtr Fc = varFactory.fluxVar("Fc");
  VarPtr Fm = varFactory.fluxVar("Fm");
  VarPtr Fe = varFactory.fluxVar("Fe");
  // variables that vary among formulations
  VarPtr Uc = varFactory.fieldVar("Uc");
  VarPtr Um = varFactory.fieldVar("Um");
  VarPtr Ue = varFactory.fieldVar("Ue");
  // create aliases for these for each formulation
  VarPtr rho = Uc;
  VarPtr u = Um;
  VarPtr T = Ue;
  VarPtr m = Um;
  VarPtr E = Ue;
  VarPtr Vc = Uc;
  VarPtr Vm = Um;
  VarPtr Ve = Ue;

  ////////////////////   INITIALIZE USEFUL VARIABLES   ///////////////////////
  // Define useful functions
  FunctionPtr one = Function::constant(1.0);
  FunctionPtr zero = Function::zero();

  // Initialize useful variables
  vector< BFPtr > bfs;
  vector< Teuchos::RCP<RHSEasy> > rhss;
  vector< IPPtr > ips;
  for (int slab=0; slab < numSlabs; slab++)
  {
    BFPtr bf = Teuchos::rcp( new BF(varFactory) );
    Teuchos::RCP<RHSEasy> rhs = Teuchos::rcp( new RHSEasy );
    IPPtr ip = Teuchos::rcp(new IP);
    bfs.push_back(bf);
    rhss.push_back(rhs);
    ips.push_back(ip);
  }

  ////////////////////   BUILD MESH   ///////////////////////
  vector< Teuchos::RCP<Mesh> > meshes;
  vector<double> tmins;
  vector<double> tmaxs;
  for (int slab=0; slab < numSlabs; slab++)
  {
    // define nodes for mesh
    FieldContainer<double> meshBoundary(4,2);
    // xmin = 0.0;
    // xmax = 1.0;
    double tminslab = tmax*double(slab)/numSlabs;
    double tmaxslab = tmax*double(slab+1)/numSlabs;

    meshBoundary(0,0) =  xmin; // x1
    meshBoundary(0,1) =  tminslab; // y1
    meshBoundary(1,0) =  xmax;
    meshBoundary(1,1) =  tminslab;
    meshBoundary(2,0) =  xmax;
    meshBoundary(2,1) =  tmaxslab;
    meshBoundary(3,0) =  xmin;
    meshBoundary(3,1) =  tmaxslab;

    // create a pointer to a new mesh:
    Teuchos::RCP<Mesh> mesh = Mesh::buildQuadMesh(meshBoundary, numX, numT,
      bfs[slab], H1Order, H1Order+pToAdd);
    meshes.push_back(mesh);
    tmins.push_back(tminslab);
    tmaxs.push_back(tmaxslab);
  }

  ////////////////////   SET INITIAL CONDITIONS   ///////////////////////
  BCPtr nullBC = Teuchos::rcp((BC*)NULL);
  RHSPtr nullRHS = Teuchos::rcp((RHS*)NULL);
  IPPtr nullIP = Teuchos::rcp((IP*)NULL);
  map<int, Teuchos::RCP<Function> > initialGuess;
  // initialGuess[rho->ID()] = Teuchos::rcp( new DiscontinuousInitialCondition(xint, rhoL, rhoR) ) ;
  // initialGuess[u->ID()]   = Teuchos::rcp( new DiscontinuousInitialCondition(xint, uL, uR) );
  // initialGuess[T->ID()]   = Teuchos::rcp( new DiscontinuousInitialCondition(xint, TL, TR) );
  initialGuess[Uc->ID()] = Teuchos::rcp( new RampedInitialCondition(xint, UcL, UcR, (xmax-xmin)/numX) ) ;
  initialGuess[Um->ID()]   = Teuchos::rcp( new RampedInitialCondition(xint, UmL, UmR,     (xmax-xmin)/numX) );
  initialGuess[Ue->ID()]   = Teuchos::rcp( new RampedInitialCondition(xint, UeL, UeR,     (xmax-xmin)/numX) );

  vector< SolutionPtr > backgroundFlows;
  for (int slab=0; slab < numSlabs; slab++)
  {
    SolutionPtr backgroundFlow = Teuchos::rcp(new Solution(meshes[slab], nullBC, nullRHS, nullIP) );
    backgroundFlow->projectOntoMesh(initialGuess);
    backgroundFlows.push_back(backgroundFlow);
  }

  ////////////////////   DEFINE BILINEAR FORM   ///////////////////////
  for (int slab=0; slab < numSlabs; slab++)
  {
    BFPtr bf = bfs[slab];
    Teuchos::RCP<RHSEasy> rhs = rhss[slab];
    IPPtr ip = ips[slab];

    FunctionPtr Uc_prev = Function::solution(Uc, backgroundFlows[slab]);
    FunctionPtr Um_prev   = Function::solution(Um, backgroundFlows[slab]);
    FunctionPtr Ue_prev   = Function::solution(Ue, backgroundFlows[slab]);
    FunctionPtr D_prev   = Function::solution(D, backgroundFlows[slab]);
    FunctionPtr rho_prev = Uc_prev;
    FunctionPtr u_prev = Um_prev;
    FunctionPtr T_prev = Ue_prev;
    // FunctionPtr T_prev = Teuchos::rcp( new BoundedBelowFunction(Ue_prev, 1e-6) );
    FunctionPtr m_prev = Um_prev;
    FunctionPtr E_prev = Ue_prev;
    FunctionPtr Vc_prev = Uc_prev;
    FunctionPtr Vm_prev = Um_prev;
    FunctionPtr Ve_prev = Ue_prev;
    FunctionPtr FEc;
    FunctionPtr FEm;
    FunctionPtr FEe;
    LinearTermPtr FEc_dU = Teuchos::rcp( new LinearTerm );
    LinearTermPtr FEm_dU = Teuchos::rcp( new LinearTerm );
    LinearTermPtr FEe_dU = Teuchos::rcp( new LinearTerm );
    LinearTermPtr M_DxS = Teuchos::rcp( new LinearTerm );
    LinearTermPtr M_qxtau = Teuchos::rcp( new LinearTerm );
    LinearTermPtr Msqrt_DxS = Teuchos::rcp( new LinearTerm );
    LinearTermPtr Msqrt_qxtau = Teuchos::rcp( new LinearTerm );
    LinearTermPtr K_DxGradV = Teuchos::rcp( new LinearTerm );
    LinearTermPtr K_qxGradV = Teuchos::rcp( new LinearTerm );
    LinearTermPtr MinvsqrtxK_DxGradV = Teuchos::rcp( new LinearTerm );
    LinearTermPtr MinvsqrtxK_qxGradV = Teuchos::rcp( new LinearTerm );
    LinearTermPtr F_cxGradV = Teuchos::rcp( new LinearTerm );
    LinearTermPtr C_cxdVdt = Teuchos::rcp( new LinearTerm );
    LinearTermPtr F_mxGradV = Teuchos::rcp( new LinearTerm );
    LinearTermPtr C_mxdVdt = Teuchos::rcp( new LinearTerm );
    LinearTermPtr F_exGradV = Teuchos::rcp( new LinearTerm );
    LinearTermPtr C_exdVdt = Teuchos::rcp( new LinearTerm );
    LinearTermPtr G_cxGradPsi = Teuchos::rcp( new LinearTerm );
    LinearTermPtr G_mxGradPsi = Teuchos::rcp( new LinearTerm );
    LinearTermPtr G_exGradPsi = Teuchos::rcp( new LinearTerm );
    FunctionPtr T_sqrt = Teuchos::rcp( new BoundedSqrtFunction(T_prev, 0.1) );
    FunctionPtr rho_sqrt = Teuchos::rcp( new BoundedSqrtFunction(rho_prev, 0.1) );
    FunctionPtr A0p_c = sqrt(gamma-1)/rho_sqrt;
    FunctionPtr A0p_m = rho_sqrt/(sqrt(Cv)*T_sqrt);
    FunctionPtr A0p_e = rho_sqrt/(T_sqrt*T_sqrt);
    // FunctionPtr invA0p_c = rho_sqrt/sqrt(gamma-1);
    // FunctionPtr invA0p_m = (sqrt(Cv)*T_sqrt)/rho_sqrt;
    // FunctionPtr invA0p_e = T_prev/rho_sqrt;
    FunctionPtr invA0p_c = 1./A0p_c;
    FunctionPtr invA0p_m = 1./A0p_m;
    FunctionPtr invA0p_e = 1./A0p_e;
    FunctionPtr div_indicator = Teuchos::rcp( new DivergenceIndicator(D_prev) );
    FunctionPtr artificialViscosity = 0.1*Function::h()*div_indicator+1e-4;
    // FunctionPtr artificialViscosity = 0.1*Function::h()+1e-4;
    // FunctionPtr artificialViscosity = 0.1*Function::h()*div_indicator + 1e-4;
    // FunctionPtr mu = artificialViscosity;
    // FunctionPtr mu_sqrt = Teuchos::rcp( new SqrtFunction(mu) );
    switch (formulation)
    {
      case 0:    
      //   /$$$$$$$            /$$               /$$   /$$     /$$                     
      //  | $$__  $$          |__/              |__/  | $$    |__/                     
      //  | $$  \ $$  /$$$$$$  /$$ /$$$$$$/$$$$  /$$ /$$$$$$   /$$ /$$    /$$  /$$$$$$ 
      //  | $$$$$$$/ /$$__  $$| $$| $$_  $$_  $$| $$|_  $$_/  | $$|  $$  /$$/ /$$__  $$
      //  | $$____/ | $$  \__/| $$| $$ \ $$ \ $$| $$  | $$    | $$ \  $$/$$/ | $$$$$$$$
      //  | $$      | $$      | $$| $$ | $$ | $$| $$  | $$ /$$| $$  \  $$$/  | $$_____/
      //  | $$      | $$      | $$| $$ | $$ | $$| $$  |  $$$$/| $$   \  $/   |  $$$$$$$
      //  |__/      |__/      |__/|__/ |__/ |__/|__/   \___/  |__/    \_/     \_______/
      //                                                                               
      //                                                                               
      //
      // Define Euler fluxes and flux jacobians
      FEc = rho_prev*u_prev;
      FEm = rho_prev*u_prev*u_prev + R*rho_prev*T_prev;
      FEe = Cv*rho_prev*u_prev*T_prev + 0.5*rho_prev*u_prev*u_prev*u_prev + R*rho_prev*u_prev*T_prev;
      FEc_dU->addTerm( u_prev*rho + rho_prev*u );
      FEm_dU->addTerm( u_prev*u_prev*rho + 2*rho_prev*u_prev*u + R*T_prev*rho + R*rho_prev*T );
      FEe_dU->addTerm( Cv*u_prev*T_prev*rho + Cv*rho_prev*T_prev*u + Cv*rho_prev*u_prev*T
            + 0.5*u_prev*u_prev*u_prev*rho + 1.5*rho_prev*u_prev*u_prev*u
            + R*rho_prev*T_prev*u + R*u_prev*T_prev*rho + R*rho_prev*u_prev*T );
      // Define norm terms
      M_DxS->addTerm( 1./mu*S );
      // Msqrt_DxS->addTerm( 1./sqrt(mu)*S );
      Msqrt_DxS->addTerm( 1./mu_sqrt*S );
      M_qxtau->addTerm( Pr/(Cp*mu)*tau );
      // Msqrt_qxtau->addTerm( sqrt(Pr/(Cp*mu))*tau );
      Msqrt_qxtau->addTerm( sqrt(Pr/Cp)/mu_sqrt*tau );
      K_DxGradV->addTerm( vm->dx() + u_prev*ve->dx() );
      // MinvsqrtxK_DxGradV->addTerm( sqrt(mu)*vm->dx() + sqrt(mu)*u_prev*ve->dx() );
      MinvsqrtxK_DxGradV->addTerm( mu_sqrt*vm->dx() + mu_sqrt*u_prev*ve->dx() );
      K_qxGradV->addTerm( -ve->dx() );
      // MinvsqrtxK_qxGradV->addTerm( -sqrt(Cp*mu/Pr)*ve->dx() );
      MinvsqrtxK_qxGradV->addTerm( -sqrt(Cp/Pr)*mu_sqrt*ve->dx() );
      F_cxGradV->addTerm( u_prev*vc->dx() + u_prev*u_prev*vm->dx() + R*T_prev*vm->dx() + Cv*T_prev*u_prev*ve->dx() 
        + 0.5*u_prev*u_prev*u_prev*ve->dx() + R*T_prev*u_prev*ve->dx() );
      C_cxdVdt->addTerm( vc->dy() + u_prev*vm->dy() + Cv*T_prev*ve->dy() + 0.5*u_prev*u_prev*ve->dy() );
      F_mxGradV->addTerm( rho_prev*vc->dx() + 2*rho_prev*u_prev*vm->dx() + Cv*T_prev*rho_prev*ve->dx() 
        + 0.5*rho_prev*u_prev*u_prev*ve->dx() + rho_prev*u_prev*u_prev*ve->dx() + R*T_prev*rho_prev*ve->dx() - D_prev*ve->dx() );
      C_mxdVdt->addTerm( rho_prev*vm->dy() + rho_prev*u_prev*ve->dy() );
      F_exGradV->addTerm( R*rho_prev*vm->dx() + Cv*rho_prev*u_prev*ve->dx() + R*rho_prev*u_prev*ve->dx() );
      C_exdVdt->addTerm( Cv*rho_prev*ve->dy() );
      G_mxGradPsi->addTerm( 2*S->dx() );
      G_exGradPsi->addTerm( -tau->dx() );

      // S terms:
      // bf->addTerm( D/mu, S);
      bf->addTerm( D/mu, S);
      bf->addTerm( 2*u, S->dx());
      bf->addTerm( -2*uhat, S->times_normal_x());

      // tau terms:
      bf->addTerm( Pr/Cp/mu*q, tau);
      bf->addTerm( -T, tau->dx());
      bf->addTerm( That, tau->times_normal_x());

      // vc terms:
      // bf->addTerm( -rho_prev*u, vc->dx());
      // bf->addTerm( -u_prev*rho, vc->dx());
      bf->addTerm( -FEc_dU, vc->dx());
      bf->addTerm( -rho, vc->dy());
      bf->addTerm( Fc, vc);

      // vm terms:
      // bf->addTerm( -rho_prev*u_prev*u, vm->dx());
      // bf->addTerm( -rho_prev*u_prev*u, vm->dx());
      // bf->addTerm( -u_prev*u_prev*rho, vm->dx());
      // bf->addTerm( -R*rho_prev*T, vm->dx());
      // bf->addTerm( -R*T_prev*rho, vm->dx());
      bf->addTerm( -FEm_dU, vm->dx());
      bf->addTerm( D, vm->dx());
      bf->addTerm( -rho_prev*u, vm->dy());
      bf->addTerm( -u_prev*rho, vm->dy());
      bf->addTerm( Fm, vm);

      // ve terms:
      // bf->addTerm( -Cv*rho_prev*T_prev*u, ve->dx());
      // bf->addTerm( -Cv*u_prev*rho_prev*T, ve->dx());
      // bf->addTerm( -Cv*T_prev*u_prev*rho, ve->dx());
      // bf->addTerm( -0.5*rho_prev*u_prev*u_prev*u, ve->dx());
      // bf->addTerm( -0.5*rho_prev*u_prev*u_prev*u, ve->dx());
      // bf->addTerm( -0.5*rho_prev*u_prev*u_prev*u, ve->dx());
      // bf->addTerm( -0.5*u_prev*u_prev*u_prev*rho, ve->dx());
      // bf->addTerm( -R*rho_prev*T_prev*u, ve->dx());
      // bf->addTerm( -R*rho_prev*u_prev*T, ve->dx());
      // bf->addTerm( -R*u_prev*T_prev*rho, ve->dx());
      bf->addTerm( -FEe_dU, ve->dx());
      bf->addTerm( -q, ve->dx());
      bf->addTerm( u_prev*D, ve->dx());
      bf->addTerm( D_prev*u, ve->dx());
      bf->addTerm( -Cv*rho_prev*T, ve->dy());
      bf->addTerm( -Cv*T_prev*rho, ve->dy());
      bf->addTerm( -0.5*rho_prev*u_prev*u, ve->dy());
      bf->addTerm( -0.5*rho_prev*u_prev*u, ve->dy());
      bf->addTerm( -0.5*u_prev*u_prev*rho, ve->dy());
      bf->addTerm( Fe, ve);

      ////////////////////   SPECIFY RHS   ///////////////////////

      // S terms:
      rhs->addTerm( -1./mu*D_prev * S );
      rhs->addTerm( -2*u_prev * S->dx() );

      // tau terms:
      rhs->addTerm( T_prev * tau->dx() );

      // vc terms:
      // rhs->addTerm( rho_prev*u_prev * vc->dx() );
      rhs->addTerm( FEc * vc->dx() );
      rhs->addTerm( rho_prev * vc->dy() );

      // vm terms:
      // rhs->addTerm( rho_prev*u_prev*u_prev * vm->dx() );
      // rhs->addTerm( R*rho_prev*T_prev * vm->dx() );
      rhs->addTerm( FEm * vm->dx() );
      rhs->addTerm( -D_prev * vm->dx() );
      rhs->addTerm( rho_prev*u_prev * vm->dy() );

      // ve terms:
      // rhs->addTerm( Cv*rho_prev*u_prev*T_prev * ve->dx() );
      // rhs->addTerm( 0.5*rho_prev*u_prev*u_prev*u_prev * ve->dx() );
      // rhs->addTerm( R*rho_prev*u_prev*T_prev * ve->dx() );
      rhs->addTerm( FEe * ve->dx() );
      rhs->addTerm( -u_prev*D_prev * ve->dx() );
      rhs->addTerm( Cv*rho_prev*T_prev * ve->dy() );
      rhs->addTerm( 0.5*rho_prev*u_prev*u_prev * ve->dy() );

      ////////////////////   DEFINE INNER PRODUCT(S)   ///////////////////////
      switch (norm)
      {
        // Automatic graph norm
        case 0:
        ips[slab] = bf->graphNorm();
        break;

        // Manual Graph Norm
        case 1:
        ip->addTerm( M_DxS + K_DxGradV );
        ip->addTerm( M_qxtau + K_qxGradV );
        ip->addTerm( G_cxGradPsi - F_cxGradV - C_cxdVdt );
        ip->addTerm( G_mxGradPsi - F_mxGradV - C_mxdVdt );
        ip->addTerm( G_exGradPsi - F_exGradV - C_exdVdt );
        ip->addTerm(vc);
        ip->addTerm(vm);
        ip->addTerm(ve);
        break;

        // Entropy Scaled Manual Graph Norm
        case 2:
        ip->addTerm( M_DxS + K_DxGradV );
        ip->addTerm( (M_qxtau + K_qxGradV) );
        ip->addTerm( invA0p_c*(G_cxGradPsi - F_cxGradV - C_cxdVdt) );
        ip->addTerm( invA0p_m*(G_mxGradPsi - F_mxGradV - C_mxdVdt) );
        ip->addTerm( invA0p_e*(G_exGradPsi - F_exGradV - C_exdVdt) );
        // ip->addTerm( vc );
        // ip->addTerm( vm );
        // ip->addTerm( ve );
        ip->addTerm( A0p_c*vc );
        ip->addTerm( A0p_m*vm );
        ip->addTerm( A0p_e*ve );
        break;

        // Original Robust Norm
        case 3:
        ip->addTerm( Msqrt_DxS );
        ip->addTerm( MinvsqrtxK_DxGradV );
        ip->addTerm( Msqrt_qxtau );
        ip->addTerm( MinvsqrtxK_qxGradV );
        ip->addTerm( G_cxGradPsi - F_cxGradV - C_cxdVdt );
        ip->addTerm( G_mxGradPsi - F_mxGradV - C_mxdVdt );
        ip->addTerm( G_exGradPsi - F_exGradV - C_exdVdt );
        ip->addTerm( F_cxGradV );
        ip->addTerm( F_mxGradV );
        ip->addTerm( F_exGradV );
        ip->addTerm( vc );
        ip->addTerm( vm );
        ip->addTerm( ve );
        break;

        // Entropy Scaled Robust Norm
        case 4:
        ip->addTerm( Msqrt_DxS );
        ip->addTerm( MinvsqrtxK_DxGradV );
        ip->addTerm( Msqrt_qxtau );
        ip->addTerm( MinvsqrtxK_qxGradV );
        ip->addTerm( invA0p_c*(G_cxGradPsi - F_cxGradV - C_cxdVdt) );
        ip->addTerm( invA0p_m*(G_mxGradPsi - F_mxGradV - C_mxdVdt) );
        ip->addTerm( invA0p_e*(G_exGradPsi - F_exGradV - C_exdVdt) );
        ip->addTerm( invA0p_c*F_cxGradV );
        ip->addTerm( invA0p_m*F_mxGradV );
        ip->addTerm( invA0p_e*F_exGradV );
        ip->addTerm( A0p_c*vc);
        ip->addTerm( A0p_m*vm);
        ip->addTerm( A0p_e*ve);
        break;

        // Robust Norm 2
        case 5:
        ip->addTerm( Msqrt_DxS );
        ip->addTerm( MinvsqrtxK_DxGradV );
        ip->addTerm( Msqrt_qxtau );
        ip->addTerm( MinvsqrtxK_qxGradV );
        ip->addTerm( G_cxGradPsi - F_cxGradV - C_cxdVdt );
        ip->addTerm( G_mxGradPsi - F_mxGradV - C_mxdVdt );
        ip->addTerm( G_exGradPsi - F_exGradV - C_exdVdt );
        ip->addTerm( F_cxGradV + C_cxdVdt );
        ip->addTerm( F_mxGradV + C_mxdVdt );
        ip->addTerm( F_exGradV + C_exdVdt );
        ip->addTerm(vc);
        ip->addTerm(vm);
        ip->addTerm(ve);
        break;

        // Entropy Scaled Robust Norm 2
        case 6:        
        ip->addTerm( Msqrt_DxS );
        ip->addTerm( MinvsqrtxK_DxGradV );
        ip->addTerm( Msqrt_qxtau );
        ip->addTerm( MinvsqrtxK_qxGradV );
        ip->addTerm( invA0p_c*(G_cxGradPsi - F_cxGradV - C_cxdVdt) );
        ip->addTerm( invA0p_m*(G_mxGradPsi - F_mxGradV - C_mxdVdt) );
        ip->addTerm( invA0p_e*(G_exGradPsi - F_exGradV - C_exdVdt) );
        ip->addTerm( invA0p_c*(F_cxGradV + C_cxdVdt) );
        ip->addTerm( invA0p_m*(F_mxGradV + C_mxdVdt) );
        ip->addTerm( invA0p_e*(F_exGradV + C_exdVdt) );
        // ip->addTerm(vc);
        // ip->addTerm(vm);
        // ip->addTerm(ve);
        ip->addTerm( A0p_c*vc);
        ip->addTerm( A0p_m*vm);
        ip->addTerm( A0p_e*ve);
        break;

        // Original Robust Norm 3
        case 7:
        ip->addTerm( Msqrt_DxS );
        ip->addTerm( MinvsqrtxK_DxGradV );
        ip->addTerm( Msqrt_qxtau );
        ip->addTerm( MinvsqrtxK_qxGradV );
        ip->addTerm( G_cxGradPsi );
        ip->addTerm( G_mxGradPsi );
        ip->addTerm( G_exGradPsi );
        ip->addTerm( F_cxGradV + C_cxdVdt );
        ip->addTerm( F_mxGradV + C_mxdVdt );
        ip->addTerm( F_exGradV + C_exdVdt );
        ip->addTerm(vc);
        ip->addTerm(vm);
        ip->addTerm(ve);
        break;

        // Entropy Scaled Robust Norm 3
        case 8:
        ip->addTerm( Msqrt_DxS );
        ip->addTerm( MinvsqrtxK_DxGradV );
        ip->addTerm( Msqrt_qxtau );
        ip->addTerm( MinvsqrtxK_qxGradV );
        ip->addTerm( invA0p_c*G_cxGradPsi );
        ip->addTerm( invA0p_m*G_mxGradPsi );
        ip->addTerm( invA0p_e*G_exGradPsi );
        ip->addTerm( invA0p_c*(F_cxGradV + C_cxdVdt) );
        ip->addTerm( invA0p_m*(F_mxGradV + C_mxdVdt) );
        ip->addTerm( invA0p_e*(F_exGradV + C_exdVdt) );
        ip->addTerm( A0p_c*vc);
        ip->addTerm( A0p_m*vm);
        ip->addTerm( A0p_e*ve);
        break;

        // Original Robust Norm 4
        case 9:
        ip->addTerm( Msqrt_DxS + MinvsqrtxK_DxGradV );
        ip->addTerm( Msqrt_qxtau + MinvsqrtxK_qxGradV );
        ip->addTerm( G_cxGradPsi - F_cxGradV - C_cxdVdt );
        ip->addTerm( G_mxGradPsi - F_mxGradV - C_mxdVdt );
        ip->addTerm( G_exGradPsi - F_exGradV - C_exdVdt );
        ip->addTerm( F_cxGradV );
        ip->addTerm( F_mxGradV );
        ip->addTerm( F_exGradV );
        ip->addTerm(vc);
        ip->addTerm(vm);
        ip->addTerm(ve);
        break;

        // Entropy Scaled Robust Norm 4
        case 10:
        ip->addTerm( Msqrt_DxS + MinvsqrtxK_DxGradV );
        ip->addTerm( Msqrt_qxtau + MinvsqrtxK_qxGradV );
        ip->addTerm( invA0p_c*(G_cxGradPsi - F_cxGradV - C_cxdVdt) );
        ip->addTerm( invA0p_m*(G_mxGradPsi - F_mxGradV - C_mxdVdt) );
        ip->addTerm( invA0p_e*(G_exGradPsi - F_exGradV - C_exdVdt) );
        ip->addTerm( invA0p_c*F_cxGradV );
        ip->addTerm( invA0p_m*F_mxGradV );
        ip->addTerm( invA0p_e*F_exGradV );
        ip->addTerm( A0p_c*vc);
        ip->addTerm( A0p_m*vm);
        ip->addTerm( A0p_e*ve);
        break;

        // Original Robust Norm 5
        case 11:
        ip->addTerm( Msqrt_DxS + MinvsqrtxK_DxGradV );
        ip->addTerm( Msqrt_qxtau + MinvsqrtxK_qxGradV );
        ip->addTerm( G_cxGradPsi - F_cxGradV - C_cxdVdt );
        ip->addTerm( G_mxGradPsi - F_mxGradV - C_mxdVdt );
        ip->addTerm( G_exGradPsi - F_exGradV - C_exdVdt );
        ip->addTerm(vc);
        ip->addTerm(vm);
        ip->addTerm(ve);
        break;

        // Entropy Scaled Robust Norm 5
        case 12:
        ip->addTerm( Msqrt_DxS + MinvsqrtxK_DxGradV );
        ip->addTerm( Msqrt_qxtau + MinvsqrtxK_qxGradV );
        ip->addTerm( invA0p_c*(G_cxGradPsi - F_cxGradV - C_cxdVdt) );
        ip->addTerm( invA0p_m*(G_mxGradPsi - F_mxGradV - C_mxdVdt) );
        ip->addTerm( invA0p_e*(G_exGradPsi - F_exGradV - C_exdVdt) );
        ip->addTerm( A0p_c*vc);
        ip->addTerm( A0p_m*vm);
        ip->addTerm( A0p_e*ve);
        break;

        // Original Robust Norm 6
        case 13:
        ip->addTerm( Msqrt_DxS + MinvsqrtxK_DxGradV );
        ip->addTerm( Msqrt_qxtau + MinvsqrtxK_qxGradV );
        ip->addTerm( G_cxGradPsi - F_cxGradV - C_cxdVdt );
        ip->addTerm( G_mxGradPsi - F_mxGradV - C_mxdVdt );
        ip->addTerm( G_exGradPsi - F_exGradV - C_exdVdt );
        ip->addTerm( F_cxGradV + C_cxdVdt );
        ip->addTerm( F_mxGradV + C_mxdVdt );
        ip->addTerm( F_exGradV + C_exdVdt );
        ip->addTerm(vc);
        ip->addTerm(vm);
        ip->addTerm(ve);
        break;

        // Entropy Scaled Robust Norm 6
        case 14:
        ip->addTerm( Msqrt_DxS + MinvsqrtxK_DxGradV );
        ip->addTerm( Msqrt_qxtau + MinvsqrtxK_qxGradV );
        ip->addTerm( invA0p_c*(G_cxGradPsi - F_cxGradV - C_cxdVdt) );
        ip->addTerm( invA0p_m*(G_mxGradPsi - F_mxGradV - C_mxdVdt) );
        ip->addTerm( invA0p_e*(G_exGradPsi - F_exGradV - C_exdVdt) );
        ip->addTerm( invA0p_c*(F_cxGradV + C_cxdVdt) );
        ip->addTerm( invA0p_m*(F_mxGradV + C_mxdVdt) );
        ip->addTerm( invA0p_e*(F_exGradV + C_exdVdt) );
        ip->addTerm( A0p_c*vc);
        ip->addTerm( A0p_m*vm);
        ip->addTerm( A0p_e*ve);
        break;

        // Robust Norm 7
        case 15:
        ip->addTerm( Msqrt_DxS );
        ip->addTerm( MinvsqrtxK_DxGradV );
        ip->addTerm( Msqrt_qxtau );
        ip->addTerm( MinvsqrtxK_qxGradV );
        ip->addTerm( G_cxGradPsi - F_cxGradV - C_cxdVdt );
        ip->addTerm( G_mxGradPsi - F_mxGradV - C_mxdVdt );
        ip->addTerm( G_exGradPsi - F_exGradV - C_exdVdt );
        ip->addTerm( F_cxGradV + C_cxdVdt );
        ip->addTerm( F_mxGradV + C_mxdVdt );
        ip->addTerm( F_exGradV + C_exdVdt );
        // ip->addTerm( 100*mu*vc->dx() );
        // ip->addTerm( 100*mu*vm->dx() );
        // ip->addTerm( 100*mu*ve->dx() );
        // ip->addTerm( 100*mu*S->dx() );
        // ip->addTerm( 100*mu*tau->dx() );
        // ip->addTerm( 100*mu*vc->dy() );
        // ip->addTerm( 100*mu*vm->dy() );
        // ip->addTerm( 100*mu*ve->dy() );
        // ip->addTerm( 100*mu*S->dy() );
        // ip->addTerm( 100*mu*tau->dy() );
        ip->addTerm( vc );
        ip->addTerm( vm );
        ip->addTerm( ve );
        break;

        // Entropy Scaled Robust Norm 7
        case 16:        
        ip->addTerm( Msqrt_DxS );
        ip->addTerm( MinvsqrtxK_DxGradV );
        ip->addTerm( Msqrt_qxtau );
        ip->addTerm( MinvsqrtxK_qxGradV );
        ip->addTerm( invA0p_c*(G_cxGradPsi - F_cxGradV - C_cxdVdt) );
        ip->addTerm( invA0p_m*(G_mxGradPsi - F_mxGradV - C_mxdVdt) );
        ip->addTerm( invA0p_e*(G_exGradPsi - F_exGradV - C_exdVdt) );
        ip->addTerm( invA0p_c*(F_cxGradV + C_cxdVdt) );
        ip->addTerm( invA0p_m*(F_mxGradV + C_mxdVdt) );
        ip->addTerm( invA0p_e*(F_exGradV + C_exdVdt) );
        // ip->addTerm(vc);
        // ip->addTerm(vm);
        // ip->addTerm(ve);
        ip->addTerm( mu*A0p_c*vc->dx() );
        ip->addTerm( mu*A0p_m*vm->dx() );
        ip->addTerm( mu*A0p_e*ve->dx() );
        ip->addTerm( A0p_c*vc);
        ip->addTerm( A0p_m*vm);
        ip->addTerm( A0p_e*ve);
        break;

        default:
        TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "invalid inner product");
      }
      break;

      case 1:      
      //    /$$$$$$                                                                           /$$     /$$                    
      //   /$$__  $$                                                                         | $$    |__/                    
      //  | $$  \__/  /$$$$$$  /$$$$$$$   /$$$$$$$  /$$$$$$   /$$$$$$  /$$    /$$  /$$$$$$  /$$$$$$   /$$  /$$$$$$  /$$$$$$$ 
      //  | $$       /$$__  $$| $$__  $$ /$$_____/ /$$__  $$ /$$__  $$|  $$  /$$/ |____  $$|_  $$_/  | $$ /$$__  $$| $$__  $$
      //  | $$      | $$  \ $$| $$  \ $$|  $$$$$$ | $$$$$$$$| $$  \__/ \  $$/$$/   /$$$$$$$  | $$    | $$| $$  \ $$| $$  \ $$
      //  | $$    $$| $$  | $$| $$  | $$ \____  $$| $$_____/| $$        \  $$$/   /$$__  $$  | $$ /$$| $$| $$  | $$| $$  | $$
      //  |  $$$$$$/|  $$$$$$/| $$  | $$ /$$$$$$$/|  $$$$$$$| $$         \  $/   |  $$$$$$$  |  $$$$/| $$|  $$$$$$/| $$  | $$
      //   \______/  \______/ |__/  |__/|_______/  \_______/|__/          \_/     \_______/   \___/  |__/ \______/ |__/  |__/
      //                                                                                                                     
      //                                                                                                                     
      //    
      // Define Euler fluxes and flux jacobians
      FEc = m_prev;
      FEm = m_prev*m_prev/rho_prev+(gamma-1)*(E_prev-0.5*m_prev*m_prev/rho_prev);
      FEe = m_prev*E_prev/rho_prev+(gamma-1)*(E_prev-0.5*m_prev*m_prev/rho_prev)*m_prev/rho_prev;
      FEc_dU->addTerm( 1*m );
      FEm_dU->addTerm( 2*m_prev/rho_prev*m - m_prev*m_prev/(rho_prev*rho_prev)*rho 
            + (gamma-1)*E - (gamma-1)*m_prev/rho_prev*m + (gamma-1)*m_prev*m_prev/(2*rho_prev*rho_prev)*rho );
      FEe_dU->addTerm( E_prev/rho_prev*E + m_prev/rho_prev*E - m_prev*E_prev/(rho_prev*rho_prev)*rho
            + (gamma-1)*m_prev/rho_prev*E + (gamma-1)*E_prev/rho_prev*m - E_prev*m_prev/(rho_prev*rho_prev)*rho
            - 1.5*(gamma-1)*m_prev*m_prev/(rho_prev*rho_prev)*m + (gamma-1)*m_prev*m_prev*m_prev/(rho_prev*rho_prev*rho_prev)*rho );
      // S terms:
      bf->addTerm( D/mu, S);
      bf->addTerm( 2/rho_prev*m, S->dx());
      bf->addTerm( -2*m_prev/(rho_prev*rho_prev)*rho, S->dx());
      bf->addTerm( -2*uhat, S->times_normal_x());

      // tau terms:
      bf->addTerm( Pr/(mu*Cp)*q, tau);
      bf->addTerm( -1/(Cv*rho_prev)*E, tau->dx());
      bf->addTerm( m_prev/(Cv*rho_prev*rho_prev)*m, tau->dx());
      bf->addTerm( -m_prev*m_prev/(2.0*Cv*rho_prev*rho_prev*rho_prev)*rho, tau->dx());
      bf->addTerm( (E_prev-0.5*m_prev*m_prev/rho_prev)/(Cv*rho_prev*rho_prev)*rho, tau->dx());
      bf->addTerm( That, tau->times_normal_x());

      // vc terms:
      // bf->addTerm( -m, vc->dx());
      bf->addTerm( -FEc_dU, vc->dx());
      bf->addTerm( -rho, vc->dy());
      bf->addTerm( Fc, vc);

      // vm terms:
      // bf->addTerm( -2.0*m_prev/rho_prev*m, vm->dx());
      // bf->addTerm( m_prev*m_prev/(rho_prev*rho_prev)*rho, vm->dx());
      // bf->addTerm( -(gamma-1)*E, vm->dx());
      // bf->addTerm( (gamma-1)*m_prev/rho_prev*m, vm->dx());
      // bf->addTerm( -(gamma-1)*m_prev*m_prev/(2*rho_prev*rho_prev)*rho, vm->dx());
      bf->addTerm( -FEm_dU, vm->dx());
      bf->addTerm( D, vm->dx());
      bf->addTerm( -m, vm->dy());
      bf->addTerm( Fm, vm);

      // ve terms:
      // bf->addTerm( -E_prev/rho_prev*m, ve->dx());
      // bf->addTerm( -m_prev/rho_prev*E, ve->dx());
      // bf->addTerm( m_prev*E_prev/(rho_prev*rho_prev)*rho, ve->dx());
      // bf->addTerm( -(gamma-1)*m_prev/rho_prev*E, ve->dx());
      // bf->addTerm( -(gamma-1)*E_prev/rho_prev*m, ve->dx());
      // bf->addTerm( (gamma-1)*m_prev*E_prev/(rho_prev*rho_prev)*rho, ve->dx());
      // bf->addTerm( 3*(gamma-1)*m_prev*m_prev/(2*rho_prev*rho_prev)*m, ve->dx());
      // bf->addTerm( -(gamma-1)*m_prev*m_prev*m_prev/(rho_prev*rho_prev*rho_prev)*rho, ve->dx());
      bf->addTerm( -FEe_dU, ve->dx());
      bf->addTerm( -q, ve->dx());
      bf->addTerm( D_prev/rho_prev*m, ve->dx());
      bf->addTerm( m_prev/rho_prev*D, ve->dx());
      bf->addTerm( -m_prev*D_prev/(rho_prev*rho_prev)*rho, ve->dx());
      bf->addTerm( -E, ve->dy());
      bf->addTerm( Fe, ve);

      ////////////////////   SPECIFY RHS   ///////////////////////

      // S terms:
      rhs->addTerm( -1./mu*D_prev * S );
      rhs->addTerm( -2*m_prev/rho_prev * S->dx() );

      // tau terms:
      rhs->addTerm( (E_prev-0.5*m_prev*m_prev/rho_prev)/(Cv*rho_prev) * tau->dx() );

      // vc terms:
      // rhs->addTerm( m_prev * vc->dx() );
      rhs->addTerm( FEc * vc->dx() );
      rhs->addTerm( rho_prev * vc->dy() );

      // vm terms:
      // rhs->addTerm( m_prev*m_prev/rho_prev * vm->dx() );
      // rhs->addTerm( (gamma-1)*(E_prev-0.5*m_prev*m_prev/rho_prev) * vm->dx() );
      rhs->addTerm( FEm * vm->dx() );
      rhs->addTerm( -D_prev * vm->dx() );
      rhs->addTerm( m_prev * vm->dy() );

      // ve terms:
      // rhs->addTerm( m_prev*E_prev/rho_prev * ve->dx() );
      // rhs->addTerm( (gamma-1)*(E_prev-0.5*m_prev*m_prev/rho_prev)*m_prev/rho_prev * ve->dx() );
      rhs->addTerm( FEe * ve->dx() );
      rhs->addTerm( -m_prev/rho_prev*D_prev * ve->dx() );
      rhs->addTerm( E_prev * ve->dy() );

      ////////////////////   DEFINE INNER PRODUCT(S)   ///////////////////////
      switch (norm)
      {
        // Automatic graph norm
        case 0:
        ips[slab] = bf->graphNorm();
        break;

        default:
        TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "invalid inner product");
      }
      break;

      case 2:
      //   /$$$$$$$$             /$$                                            
      //  | $$_____/            | $$                                            
      //  | $$       /$$$$$$$  /$$$$$$    /$$$$$$   /$$$$$$   /$$$$$$  /$$   /$$
      //  | $$$$$   | $$__  $$|_  $$_/   /$$__  $$ /$$__  $$ /$$__  $$| $$  | $$
      //  | $$__/   | $$  \ $$  | $$    | $$  \__/| $$  \ $$| $$  \ $$| $$  | $$
      //  | $$      | $$  | $$  | $$ /$$| $$      | $$  | $$| $$  | $$| $$  | $$
      //  | $$$$$$$$| $$  | $$  |  $$$$/| $$      |  $$$$$$/| $$$$$$$/|  $$$$$$$
      //  |________/|__/  |__/   \___/  |__/       \______/ | $$____/  \____  $$
      //                                                    | $$       /$$  | $$
      //                                                    | $$      |  $$$$$$/
      //                                                    |__/       \______/ 
      // define alpha from notes
      FunctionPtr VePow1 = Teuchos::rcp( new PowerFunction(-Ve_prev, gamma));
      FunctionPtr VePow2 = Teuchos::rcp( new PowerFunction(-Ve_prev, -1.-gamma));
      FunctionPtr alphaPow1 = Teuchos::rcp( new PowerFunction((gamma-1)/VePow1, 1./(gamma-1)));
      FunctionPtr alphaPow2 = Teuchos::rcp( new PowerFunction((gamma-1)/VePow1, (2-gamma)/(gamma-1)));
      FunctionPtr alphaExp = Teuchos::rcp( new ExpFunction((-gamma+Vc_prev-0.5*Vm_prev*Vm_prev/Ve_prev)/(gamma-1)) );
      FunctionPtr alpha = alphaPow1*alphaExp;
      LinearTermPtr alpha_dU = Teuchos::rcp( new LinearTerm );
      alpha_dU->addTerm( alphaPow2*gamma*VePow2*alphaExp*Ve );
      alpha_dU->addTerm( alphaPow1*alphaExp/(gamma-1)*Vc );
      alpha_dU->addTerm( -alphaPow1*alphaExp/(gamma-1)*Vm_prev/Ve_prev*Vm );
      alpha_dU->addTerm( alphaPow1*alphaExp/(gamma-1)*Vm_prev*Vm_prev/(2*Ve_prev*Ve_prev)*Ve );

      // Define Euler fluxes and flux jacobians
      FEc = alpha*Vm_prev;
      FEm = alpha*(-Vm_prev*Vm_prev/Ve_prev+(gamma-1));
      FEe = alpha*Vm_prev/Ve_prev*(0.5*Vm_prev*Vm_prev/Ve_prev-gamma);

      FEc_dU->addTerm( Vm_prev*alpha_dU + alpha*Vm );

      FEm_dU->addTerm( (-Vm_prev*Vm_prev/Ve_prev+(gamma-1))*alpha_dU
            - 2*alpha*Vm_prev/Ve_prev*Vm + alpha*Vm_prev*Vm_prev/(Ve_prev*Ve_prev)*Ve );
      
      FEe_dU->addTerm( Vm_prev/Ve_prev*(0.5*Vm_prev*Vm_prev/Ve_prev-gamma)*alpha_dU
            + alpha*(0.5*Vm_prev*Vm_prev/Ve_prev-gamma)/Ve_prev*Vm
            - alpha*Vm_prev/(Ve_prev*Ve_prev)*(0.5*Vm_prev*Vm_prev/Ve_prev-gamma)*Ve
            + alpha*Vm_prev*Vm_prev/(Ve_prev*Ve_prev)*Vm
            - alpha*Vm_prev*Vm_prev*Vm_prev/(2*Ve_prev*Ve_prev*Ve_prev)*Ve );

      // S terms:
      bf->addTerm( D/mu, S);
      bf->addTerm( -2/Ve_prev*Vm, S->dx());
      bf->addTerm( 2*Vm_prev/(Ve_prev*Ve_prev)*Ve, S->dx());
      bf->addTerm( -2*uhat, S->times_normal_x());

      // tau terms:
      bf->addTerm( Pr/(mu*Cp)*q, tau);
      bf->addTerm( -1/(Cv*Ve_prev*Ve_prev)*Ve, tau->dx());
      bf->addTerm( That, tau->times_normal_x());

      // vc terms:
      // bf->addTerm( -Vm_prev*alpha_dU, vc->dx());
      // bf->addTerm( -alpha*Vm, vc->dx());
      bf->addTerm( -FEc_dU, vc->dx());
      bf->addTerm( Ve_prev*alpha_dU + alpha*Ve, vc->dy());
      bf->addTerm( Fc, vc);

      // vm terms:
      // bf->addTerm( (Vm_prev*Vm_prev/Ve_prev-(gamma-1))*alpha_dU, vm->dx());
      // bf->addTerm( 2*alpha*Vm_prev/Ve_prev*Vm, vm->dx());
      // bf->addTerm( -alpha*Vm_prev*Vm_prev/(Ve_prev*Ve_prev)*Ve, vm->dx());
      bf->addTerm( -FEm_dU, vm->dx());
      bf->addTerm( D, vm->dx());
      bf->addTerm( -Vm_prev*alpha_dU-alpha*Vm, vm->dy());
      bf->addTerm( Fm, vm);

      // ve terms:
      // bf->addTerm( -Vm_prev/Ve_prev*(0.5*Vm_prev*Vm_prev/Ve_prev-gamma)*alpha_dU, ve->dx());
      // bf->addTerm( -alpha*(0.5*Vm_prev*Vm_prev/Ve_prev-gamma)/Ve_prev*Vm, ve->dx());
      // bf->addTerm( alpha*Vm_prev/(Ve_prev*Ve_prev)*(0.5*Vm_prev*Vm_prev/Ve_prev-gamma)*Ve, ve->dx());
      // bf->addTerm( -alpha*Vm_prev*Vm_prev/(Ve_prev*Ve_prev)*Vm, ve->dx());
      // bf->addTerm( alpha*Vm_prev*Vm_prev*Vm_prev/(2*Ve_prev*Ve_prev*Ve_prev)*Ve, ve->dx());
      bf->addTerm( -FEe_dU, ve->dx());
      bf->addTerm( -D_prev/Ve_prev*Vm, ve->dx());
      bf->addTerm( -Vm_prev/Ve_prev*D, ve->dx());
      bf->addTerm( Vm_prev*D_prev/(Ve_prev*Ve_prev)*Ve, ve->dx());
      bf->addTerm( -q, ve->dx());
      bf->addTerm( -(1-0.5*Vm_prev*Vm_prev/Ve_prev)*alpha_dU, ve->dy());
      bf->addTerm( alpha*Vm_prev/Ve_prev*Vm, ve->dy());
      bf->addTerm( -0.5*alpha*Vm_prev*Vm_prev/(Ve_prev*Ve_prev)*Ve, ve->dy());
      bf->addTerm( Fe, ve);

      ////////////////////   SPECIFY RHS   ///////////////////////

      // S terms:
      rhs->addTerm( -1./mu*D_prev * S );
      rhs->addTerm( 2*Vm_prev/Ve_prev * S->dx() );

      // tau terms:
      rhs->addTerm( -1./(Cv*Ve_prev) * tau->dx() );

      // vc terms:
      // rhs->addTerm( alpha*Vm_prev * vc->dx() );
      rhs->addTerm( FEc * vc->dx() );
      rhs->addTerm( -alpha*Ve_prev * vc->dy() );

      // vm terms:
      // rhs->addTerm( alpha*(-Vm_prev*Vm_prev/Ve_prev + (gamma-1)) * vm->dx() );
      rhs->addTerm( FEm * vm->dx() );
      rhs->addTerm( -D_prev * vm->dx() );
      rhs->addTerm( alpha*Vm_prev * vm->dy() );

      // ve terms:
      // rhs->addTerm( alpha*Vm_prev/Ve_prev*(0.5*Vm_prev*Vm_prev/Ve_prev-gamma) * ve->dx() );
      rhs->addTerm( FEe * ve->dx() );
      rhs->addTerm( Vm_prev*D_prev/Ve_prev * ve->dx() );
      rhs->addTerm( alpha*(1-0.5*Vm_prev*Vm_prev/Ve_prev) * ve->dy() );

      ////////////////////   DEFINE INNER PRODUCT(S)   ///////////////////////
      switch (norm)
      {
        // Automatic graph norm
        case 0:
        ips[slab] = bf->graphNorm();
        break;

        default:
        TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "invalid inner product");
      }
      break;
    }
  }

  ////////////////////   CREATE BCs   ///////////////////////
  vector< Teuchos::RCP<BCEasy> > bcs;
  // FunctionPtr Fc_prev;
  // FunctionPtr Fm_prev;
  // FunctionPtr Fe_prev;
  for (int slab=0; slab < numSlabs; slab++)
  {
    Teuchos::RCP<BCEasy> bc = Teuchos::rcp( new BCEasy );
    SpatialFilterPtr left = Teuchos::rcp( new ConstantXBoundary(xmin) );
    SpatialFilterPtr right = Teuchos::rcp( new ConstantXBoundary(xmax) );
    SpatialFilterPtr init = Teuchos::rcp( new ConstantYBoundary(tmins[slab]) );
    FunctionPtr rho0  = Teuchos::rcp( new DiscontinuousInitialCondition(xint, rhoL, rhoR) );
    FunctionPtr mom0 = Teuchos::rcp( new DiscontinuousInitialCondition(xint, uL*rhoL, uR*rhoR) );
    FunctionPtr E0    = Teuchos::rcp( new DiscontinuousInitialCondition(xint, (rhoL*Cv*TL+0.5*rhoL*uL*uL), (rhoR*Cv*TR+0.5*rhoR*uR*uR)) );
    if (problem == 6)
      E0 = Teuchos::rcp( new PulseInitialCondition(0.25, 2, 1) );
    // FunctionPtr rho0  = Teuchos::rcp( new RampedInitialCondition(xint, rhoL, rhoR, (xmax-xmin)/numX) );
    // FunctionPtr mom0 = Teuchos::rcp( new RampedInitialCondition(xint, uL*rhoL, uR*rhoR, (xmax-xmin)/numX) );
    // FunctionPtr E0    = Teuchos::rcp( new RampedInitialCondition(xint, (rhoL*Cv*TL+0.5*rhoL*uL*uL), (rhoR*Cv*TR+0.5*rhoR*uR*uR), (xmax-xmin)/numX) );
    bc->addDirichlet(Fc, left, -rhoL*uL*one);
    bc->addDirichlet(Fc, right, rhoR*uR*one);
    bc->addDirichlet(Fm, left, -(rhoL*uL*uL+R*rhoL*TL)*one);
    bc->addDirichlet(Fm, right, (rhoR*uR*uR+R*rhoR*TR)*one);
    bc->addDirichlet(Fe, left, -(rhoL*Cv*TL+0.5*rhoL*uL*uL+R*rhoL*TL)*uL*one);
    bc->addDirichlet(Fe, right, (rhoR*Cv*TR+0.5*rhoR*uR*uR+R*rhoR*TR)*uR*one);
    // cout << "R = " << R << " Cv = " << Cv << " Cp = " << Cp << " gamma = " << gamma << endl;
    // cout << "left " << rhoL*uL << " " << (rhoL*uL*uL+R*rhoL*TL) << " " << (rhoL*Cv*TL+0.5*rhoL*uL*uL+R*Cv*TL)*uL << endl;
    if (slab == 0)
    {
      bc->addDirichlet(Fc, init, -rho0);
      bc->addDirichlet(Fm, init, -mom0);
      bc->addDirichlet(Fe, init, -E0);
    }
    bcs.push_back(bc);
  }

  // rhs->addTerm( -1./mu*D_prev * S );
  // rhs->addTerm( -4./3*m_prev/rho_prev * S->dx() );
  // cout << 

  ////////////////////   SOLVE & REFINE   ///////////////////////
  vector< Teuchos::RCP<Solution> > solutions;
  for (int slab=0; slab < numSlabs; slab++)
  {
    Teuchos::RCP<Solution> solution = Teuchos::rcp( new Solution(meshes[slab], bcs[slab], rhss[slab], ips[slab]) );
    solutions.push_back(solution);
    if (slab > 0)
    {
      // fhat_prev = Function::solution(fhat, solutions[slab-1]);
      FunctionPtr rho_prev = Teuchos::RCP<Function>( new PreviousSolutionFunction(backgroundFlows[slab-1], rho) );
      FunctionPtr u_prev = Teuchos::RCP<Function>( new PreviousSolutionFunction(backgroundFlows[slab-1], u) );
      FunctionPtr T_prev = Teuchos::RCP<Function>( new PreviousSolutionFunction(backgroundFlows[slab-1], T) );
      SpatialFilterPtr init = Teuchos::rcp( new ConstantYBoundary(tmins[slab]) );
      bcs[slab]->addDirichlet(Fc, init, -rho_prev);
      bcs[slab]->addDirichlet(Fm, init, -rho_prev*u_prev);
      bcs[slab]->addDirichlet(Fe, init, -Cv*rho_prev*T_prev-0.5*rho_prev*u_prev*u_prev);
    }
    meshes[slab]->registerSolution(backgroundFlows[slab]);
    meshes[slab]->registerSolution(solutions[slab]);
    double energyThreshold = 0.2; // for mesh refinements
    RefinementStrategy refinementStrategy( solution, energyThreshold );
    VTKExporter exporter(backgroundFlows[slab], meshes[slab], varFactory);
    set<int> nonlinearVars;
    nonlinearVars.insert(D->ID());
    nonlinearVars.insert(Uc->ID());
    nonlinearVars.insert(Um->ID());
    nonlinearVars.insert(Ue->ID());

    for (int refIndex=0; refIndex<=numRefs; refIndex++)
    {
      double L2Update = 1e7;
      int iterCount = 0;
      while (L2Update > nlTol && iterCount < maxNewtonIterations)
      {
        solution->condensedSolve();
        double rhoL2Update = solution->L2NormOfSolutionGlobal(rho->ID());
        double uL2Update = solution->L2NormOfSolutionGlobal(u->ID());
        double TL2Update = solution->L2NormOfSolutionGlobal(T->ID());
        L2Update = sqrt(rhoL2Update*rhoL2Update + uL2Update*uL2Update + TL2Update*TL2Update);

        // line search algorithm
        double alpha = 1.0;
        // bool useLineSearch = true;
        // amount of enriching of grid points on which to ensure positivity
        int posEnrich = 5; 
        if (useLineSearch)
        {
          double lineSearchFactor = .5; 
          double eps = .001;
          FunctionPtr rhoTemp = Function::solution(rho,backgroundFlows[slab]) + alpha*Function::solution(rho,solution) - Function::constant(eps);
          FunctionPtr TTemp = Function::solution(T,backgroundFlows[slab]) + alpha*Function::solution(T,solution) - Function::constant(eps);
          bool rhoIsPositive = rhoTemp->isPositive(meshes[slab],posEnrich);
          bool TIsPositive = TTemp->isPositive(meshes[slab],posEnrich);
          // bool TIsPositive = true;
          int iter = 0; int maxIter = 20;
          while (!(rhoIsPositive && TIsPositive) && iter < maxIter)
          {
            alpha = alpha*lineSearchFactor;
            rhoTemp = Function::solution(rho,backgroundFlows[slab]) + alpha*Function::solution(rho,solution);
            TTemp = Function::solution(T,backgroundFlows[slab]) + alpha*Function::solution(T,solution);
            rhoIsPositive = rhoTemp->isPositive(meshes[slab],posEnrich);
            TIsPositive = TTemp->isPositive(meshes[slab],posEnrich);
            // TIsPositive = true;
            iter++;
          }
          if (commRank==0 && alpha < 1.0){
            cout << "line search factor alpha = " << alpha << endl;
          }
        }

        backgroundFlows[slab]->addSolution(solution, alpha, nonlinearVars);
        iterCount++;
        if (commRank == 0)
          cout << "L2 Norm of Update = " << L2Update << endl;
        if (alpha <= 1./128)
          break;
      }
      if (commRank == 0)
        cout << endl;

      if (commRank == 0)
      {
        stringstream outfile;
        outfile << problemName << norm << "_" << slab << "_" << refIndex;
        exporter.exportSolution(outfile.str());
        FunctionPtr D_prev = Function::solution(D,backgroundFlows[slab]);
        FunctionPtr div_indicator = Teuchos::rcp( new DivergenceIndicator(D_prev) );
        FunctionPtr errorFunction = Teuchos::rcp( new ErrorFunction(solution) );
        // exporter.exportFunction(div_indicator,"Indicator"+Teuchos::toString(refIndex));
        // exporter.exportFunction(errorFunction,"Error"+Teuchos::toString(refIndex));
      }

      if (refIndex < numRefs)
      {
        refinementStrategy.refine(commRank==0);
        double newRamp = (xmax-xmin)/(numX*pow(2., refIndex+1));
        // if (commRank == 0)
        //   cout << "New ramp width = " << newRamp << endl;
        // dynamic_cast< RampedInitialCondition* >(initialGuess[rho->ID()].get())->setH(newRamp);
        // dynamic_cast< RampedInitialCondition* >(initialGuess[u->ID()].get())->setH(newRamp);
        // dynamic_cast< RampedInitialCondition* >(initialGuess[T->ID()].get())->setH(newRamp);
        // dynamic_cast< RampedInitialCondition* >(rho0.get())->setH(newRamp);
        // dynamic_cast< RampedInitialCondition* >(mom0.get())->setH(newRamp);
        // dynamic_cast< RampedInitialCondition* >(E0.get())->setH(newRamp);
        // backgroundFlow->projectOntoMesh(initialGuess);
      }
    }
  }

  return 0;
}
