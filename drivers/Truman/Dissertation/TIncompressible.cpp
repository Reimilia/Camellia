#include "EnergyErrorFunction.h"
#include "Function.h"
#include "GMGSolver.h"
#include "HDF5Exporter.h"
#include "MeshFactory.h"
#include "NavierStokesVGPFormulation.h"
#include "SimpleFunction.h"
#include "ExpFunction.h"
#include "TrigFunctions.h"
#include "SuperLUDistSolver.h"

#include "Teuchos_GlobalMPISession.hpp"

using namespace Camellia;

// this Function will work for both 2D and 3D cavity flow top BC (matching y = 1)
class RampBoundaryFunction_U1 : public SimpleFunction<double>
{
  double _eps; // ramp width
public:
  RampBoundaryFunction_U1(double eps)
  {
    _eps = eps;
  }
  double value(double x, double y)
  {
    double tol = 1e-14;
    if (abs(y-1.0) < tol)   // top boundary
    {
      if ( (abs(x) < _eps) )   // top left
      {
        return x / _eps;
      }
      else if ( abs(1.0-x) < _eps)     // top right
      {
        return (1.0-x) / _eps;
      }
      else     // top middle
      {
        return 1;
      }
    }
    else     // not top boundary: 0.0
    {
      return 0.0;
    }
  }
  double value(double x, double y, double z)
  {
    // bilinear interpolation with ramp of width _eps around top edges
    double tol = 1e-14;
    if (abs(y-1.0) <tol)
    {
      double xFactor = 1.0;
      double zFactor = 1.0;
      if ( (abs(x) < _eps) )   // top left
      {
        xFactor = x / _eps;
      }
      else if ( abs(1.0-x) < _eps)     // top right
      {
        xFactor = (1.0-x) / _eps;
      }
      if ( (abs(z) < _eps) )   // top back
      {
        zFactor = z / _eps;
      }
      else if ( abs(1.0-z) < _eps)     // top front
      {
        zFactor = (1.0-z) / _eps;
      }
      return xFactor * zFactor;
    }
    else
    {
      return 0.0;
    }
  }
};

class TimeRamp : public SimpleFunction<double>
{
  FunctionPtr _time;
  double _timeScale;
  double getTimeValue()
  {
    ParameterFunction* timeParamFxn = dynamic_cast<ParameterFunction*>(_time.get());
    SimpleFunction<double>* timeFxn = dynamic_cast<SimpleFunction<double>*>(timeParamFxn->getValue().get());
    return timeFxn->value(0);
  }
public:
  TimeRamp(FunctionPtr timeConstantParamFxn, double timeScale)
  {
    _time = timeConstantParamFxn;
    _timeScale = timeScale;
  }
  double value(double x)
  {
    double t = getTimeValue();
    if (t >= _timeScale)
    {
      return 1.0;
    }
    else
    {
      return t / _timeScale;
    }
  }
};

using namespace std;

void setDirectSolver(NavierStokesVGPFormulation &form)
{
  Teuchos::RCP<Solver> coarseSolver = Solver::getDirectSolver(true);
#if defined(HAVE_AMESOS_SUPERLUDIST) || defined(HAVE_AMESOS2_SUPERLUDIST)
  SuperLUDistSolver* superLUSolver = dynamic_cast<SuperLUDistSolver*>(coarseSolver.get());
  if (superLUSolver)
  {
    superLUSolver->setRunSilent(true);
  }
#endif
  form.setSolver(coarseSolver);
}

void setGMGSolver(NavierStokesVGPFormulation &form, vector<MeshPtr> &meshesCoarseToFine,
                                     int cgMaxIters, double cgTol, bool useCondensedSolve)
{
  Teuchos::RCP<Solver> coarseSolver = Solver::getDirectSolver(true);
  Teuchos::RCP<GMGSolver> gmgSolver = Teuchos::rcp( new GMGSolver(form.solutionIncrement(), meshesCoarseToFine, cgMaxIters, cgTol,
                                                                  GMGOperator::V_CYCLE, coarseSolver, useCondensedSolve) );
  gmgSolver->setAztecOutput(0);
#if defined(HAVE_AMESOS_SUPERLUDIST) || defined(HAVE_AMESOS2_SUPERLUDIST)
  SuperLUDistSolver* superLUSolver = dynamic_cast<SuperLUDistSolver*>(coarseSolver.get());
  if (superLUSolver)
  {
    superLUSolver->setRunSilent(true);
  }
#endif
  form.setSolver(gmgSolver);
}

int main(int argc, char *argv[])
{
  Teuchos::GlobalMPISession mpiSession(&argc, &argv); // initialize MPI
  int rank = Teuchos::GlobalMPISession::getRank();

#ifdef HAVE_MPI
  Epetra_MpiComm Comm(MPI_COMM_WORLD);
#else
  Epetra_SerialComm Comm;
#endif
  Comm.Barrier(); // set breakpoint here to allow debugger attachment to other MPI processes than the one you automatically attached to.

  Teuchos::CommandLineProcessor cmdp(false,true); // false: don't throw exceptions; true: do return errors for unrecognized options

  // Set Parameters
  string problemName = "Trivial";
  cmdp.setOption("problem", &problemName, "LidDriven, HemkerCylinder");
  bool steady = true;
  cmdp.setOption("steady", "unsteady", &steady, "steady");
  string outputDir = ".";
  cmdp.setOption("outputDir", &outputDir, "output directory");
  int spaceDim = 2;
  cmdp.setOption("spaceDim", &spaceDim, "spatial dimension");
  double Re = 1e2;
  cmdp.setOption("Re", &Re, "Re");
  string norm = "Graph";
  cmdp.setOption("norm", &norm, "norm");
  bool useDirectSolver = false; // false has an issue during GMGOperator::setFineStiffnessMatrix's call to GMGOperator::computeCoarseStiffnessMatrix().  I'm not yet clear on the nature of this isssue.
  cmdp.setOption("useDirectSolver", "useIterativeSolver", &useDirectSolver, "use direct solver");
  bool useCondensedSolve = false;
  cmdp.setOption("useCondensedSolve", "useStandardSolve", &useCondensedSolve);
  bool useConformingTraces = false;
  cmdp.setOption("conformingTraces", "nonconformingTraces", &useConformingTraces, "use conforming traces");
  int polyOrder = 2, delta_k = 2;
  cmdp.setOption("polyOrder",&polyOrder,"polynomial order for field variable u");
  int polyOrderCoarse = 1;
  double cgTol = 1e-6;
  cmdp.setOption("cgTol", &cgTol, "iterative solver tolerance");
  int cgMaxIters = 2000;
  cmdp.setOption("cgMaxIters", &cgMaxIters, "maximum number of iterations for linear solver");
  double nlTol = 1e-6;
  cmdp.setOption("nlTol", &nlTol, "Newton iteration tolerance");
  int nlMaxIters = 10;
  cmdp.setOption("nlMaxIters", &nlMaxIters, "maximum number of iterations for Newton solve");
  int numRefs = 10;
  cmdp.setOption("numRefs",&numRefs,"number of refinements");

  if (cmdp.parse(argc,argv) != Teuchos::CommandLineProcessor::PARSE_SUCCESSFUL)
  {
#ifdef HAVE_MPI
    MPI_Finalize();
#endif
    return -1;
  }

  vector<double> pressureConstraintPoint;
  
  // Construct Mesh
  MeshTopologyPtr meshTopo;
  if (problemName == "Trivial")
  {
    int meshWidth = 2;
    vector<double> dims(spaceDim,1.0);
    vector<int> numElements(spaceDim,meshWidth);
    vector<double> x0(spaceDim,0.0);
    
    pressureConstraintPoint = {0.5,0.5};
    
    meshTopo = MeshFactory::rectilinearMeshTopology(dims,numElements,x0);
    if (!steady)
    {
      double t0 = 0;
      double t1 = 1;
      int temporalDivisions = 2;
      meshTopo = MeshFactory::spaceTimeMeshTopology(meshTopo, t0, t1, temporalDivisions);
    }
  }
  else if (problemName == "LidDriven")
  {
    int meshWidth = 2;
    vector<double> dims(spaceDim,1.0);
    vector<int> numElements(spaceDim,meshWidth);
    vector<double> x0(spaceDim,0.0);
    pressureConstraintPoint = {0.5,0.5};
    meshTopo = MeshFactory::rectilinearMeshTopology(dims,numElements,x0);
    if (!steady)
    {
      double t0 = 0;
      double t1 = 1;
      int temporalDivisions = 2;
      meshTopo = MeshFactory::spaceTimeMeshTopology(meshTopo, t0, t1, temporalDivisions);
    }
  }
  else if (problemName == "Kovasznay")
  {
    vector<double> x0;
    vector<double> dims;
    vector<int> numElements;
    x0.push_back(-.5);
    x0.push_back(-.5);
    dims.push_back(1.5);
    dims.push_back(2.0);
    numElements.push_back(3);
    numElements.push_back(4);
    pressureConstraintPoint = x0;
    meshTopo = MeshFactory::rectilinearMeshTopology(dims,numElements,x0);
    if (!steady)
    {
      double t0 = 0;
      double t1 = 1;
      int temporalDivisions = 2;
      meshTopo = MeshFactory::spaceTimeMeshTopology(meshTopo, t0, t1, temporalDivisions);
    }
  }

  Teuchos::ParameterList nsParameters;
  if (steady)
    nsParameters = NavierStokesVGPFormulation::steadyConservationFormulation(spaceDim, Re, useConformingTraces, meshTopo, polyOrder, delta_k).getConstructorParameters();
  else
    nsParameters = NavierStokesVGPFormulation::spaceTimeConservationFormulation(spaceDim, Re, useConformingTraces, meshTopo, polyOrder, polyOrder, delta_k).getConstructorParameters();
  
  nsParameters.set("neglectFluxesOnRHS", false);
  NavierStokesVGPFormulation form(meshTopo, nsParameters);

  form.solutionIncrement()->setUseCondensedSolve(useCondensedSolve);
  if (problemName == "Trivial")
    form.addPointPressureCondition(pressureConstraintPoint);
  if (problemName == "LidDriven")
    form.addPointPressureCondition(pressureConstraintPoint);
  if (problemName == "Kovasznay")
    form.addPointPressureCondition(pressureConstraintPoint);

  MeshPtr mesh = form.solutionIncrement()->mesh();
  vector<MeshPtr> meshesCoarseToFine = GMGSolver::meshesForMultigrid(mesh, polyOrderCoarse, delta_k);

  int numberOfMeshesForMultigrid = meshesCoarseToFine.size();

  VarPtr u1_hat = form.u_hat(1), u2_hat = form.u_hat(2);
  VarPtr tn1_hat = form.tn_hat(1), tn2_hat = form.tn_hat(2);

  if (problemName == "Trivial")
  {
    BCPtr bc = form.solutionIncrement()->bc();

    SpatialFilterPtr leftX  = SpatialFilter::matchingX(0);
    SpatialFilterPtr rightX = SpatialFilter::matchingX(1);
    SpatialFilterPtr leftY  = SpatialFilter::matchingY(0);
    SpatialFilterPtr rightY = SpatialFilter::matchingY(1);
    SpatialFilterPtr t0  = SpatialFilter::matchingT(0);

    FunctionPtr u1_exact = Function::constant(1);
    FunctionPtr u2_exact = Function::zero();
    FunctionPtr u_exact = Function::vectorize(u1_exact,u2_exact);
    form.addInflowCondition(leftX,  u_exact);
    form.addInflowCondition(rightX, u_exact);
    form.addInflowCondition(leftY,  u_exact);
    form.addInflowCondition(rightY, u_exact);

    if (!steady)
      form.addFluxCondition(t0, -u_exact);
  }
  else if (problemName == "LidDriven")
  {
    SpatialFilterPtr topBoundary = SpatialFilter::matchingY(1);
    SpatialFilterPtr notTopBoundary = SpatialFilter::negatedFilter(topBoundary);
    form.addWallCondition(notTopBoundary);

    double eps = 1.0 / 64.0;
    FunctionPtr u1_topRamp = Teuchos::rcp( new RampBoundaryFunction_U1(eps) );
    FunctionPtr u_topRamp;
    FunctionPtr zero = Function::zero();
    if (spaceDim == 2)
    {
      u_topRamp = Function::vectorize(u1_topRamp,zero);
    }
    else
    {
      u_topRamp = Function::vectorize(u1_topRamp,zero,zero);
    }
    form.addInflowCondition(topBoundary, u_topRamp);
  }
  else if (problemName == "Kovasznay")
  {
    BCPtr bc = form.solutionIncrement()->bc();

    SpatialFilterPtr leftX  = SpatialFilter::matchingX(-.5);
    SpatialFilterPtr rightX = SpatialFilter::matchingX(1);
    SpatialFilterPtr leftY  = SpatialFilter::matchingY(-.5);
    SpatialFilterPtr rightY = SpatialFilter::matchingY(1.5);
    // SpatialFilterPtr allSpace = SpatialFilter::allSpace();
    SpatialFilterPtr t0  = SpatialFilter::matchingT(0);

    double pi = atan(1)*4;
    double lambda = Re/2-sqrt(Re*Re/4+4*pi*pi);
    FunctionPtr explambdaX = Teuchos::rcp(new Exp_ax(lambda));
    FunctionPtr cos2piY = Teuchos::rcp(new Cos_ay(2*pi));
    FunctionPtr sin2piY = Teuchos::rcp(new Sin_ay(2*pi));
    FunctionPtr u1_exact = 1 - explambdaX*cos2piY;
    FunctionPtr u2_exact = lambda/(2*pi)*explambdaX*sin2piY;
    FunctionPtr sigma1_exact = 1./Re*u1_exact->grad();
    FunctionPtr sigma2_exact = 1./Re*u2_exact->grad();

    FunctionPtr u_exact = Function::vectorize(u1_exact,u2_exact);
    // form.addInflowCondition(allSpace, u_exact);
    form.addInflowCondition(leftX,  u_exact);
    form.addInflowCondition(rightX, u_exact);
    form.addInflowCondition(leftY,  u_exact);
    form.addInflowCondition(rightY, u_exact);

    if (!steady)
      form.addFluxCondition(t0, -u_exact);
    // bc->addDirichlet(u1_hat, leftX,  u1_exact);
    // bc->addDirichlet(u2_hat, leftX,  u2_exact);
    // bc->addDirichlet(u1_hat, rightX, u1_exact);
    // bc->addDirichlet(u2_hat, rightX, u2_exact);
    // bc->addDirichlet(u1_hat, leftY,  u1_exact);
    // bc->addDirichlet(u2_hat, leftY,  u2_exact);
    // bc->addDirichlet(u1_hat, rightY, u1_exact);
    // bc->addDirichlet(u2_hat, rightY, u2_exact);

    // bc->addDirichlet(u1_hat, allSpace,  u1_exact);
    // bc->addDirichlet(u2_hat, allSpace,  u2_exact);
  }

  double l2NormOfIncrement = 1.0;
  int stepNumber = 0;

  cout << setprecision(2) << scientific;

  while ((l2NormOfIncrement > nlTol) && (stepNumber < nlMaxIters))
  {
    if (useDirectSolver)
      setDirectSolver(form);
    else
      setGMGSolver(form, meshesCoarseToFine, cgMaxIters, cgTol, useCondensedSolve);

    form.solveAndAccumulate();
    l2NormOfIncrement = form.L2NormSolutionIncrement();
    stepNumber++;

    if (rank==0) cout << stepNumber << ". L^2 norm of increment: " << l2NormOfIncrement;

    if (!useDirectSolver)
    {
      Teuchos::RCP<GMGSolver> gmgSolver = Teuchos::rcp(dynamic_cast<GMGSolver*>(form.getSolver().get()), false);
      int iterationCount = gmgSolver->iterationCount();
      if (rank==0) cout << " (" << iterationCount << " GMG iterations)\n";
    }
    else
    {
      cout << endl;
    }
  }
  form.clearSolutionIncrement(); // need to clear before evaluating energy error
  
  FunctionPtr energyErrorFunction = EnergyErrorFunction::energyErrorFunction(form.solutionIncrement());

  ostringstream exportName;
  if (steady)
    exportName << "Steady";
  else
    exportName << "Transient";
  exportName << problemName << "_Re" << Re << "_" << norm << "_k" << polyOrder;// << "_" << solverChoice;// << "_" << multigridStrategyString;
  HDF5Exporter exporter(form.solution()->mesh(), exportName.str(), outputDir);

  exportName << "_energyError";
  HDF5Exporter energyErrorExporter(form.solution()->mesh(), exportName.str(), outputDir);

  exporter.exportSolution(form.solution(), 0);
  energyErrorExporter.exportFunction(energyErrorFunction, "energy error", 0);

  double energyError = form.solutionIncrement()->energyErrorTotal();
  int globalDofs = mesh->globalDofCount();
  int activeElements = mesh->getTopology()->getActiveCellIndices().size();
  if (rank==0) cout << "Initial energy error: " << energyError;
  if (rank==0) cout << " (mesh has " << activeElements << " elements and " << globalDofs << " global dofs)." << endl;

  bool truncateMultigridMeshes = true; // for getting a "fair" sense of how iteration counts vary with h.

  double tol = 1e-5;
  int refNumber = 0;
  do
  {
    refNumber++;
    form.refine();

    if (rank==0) cout << " ****** Refinement " << refNumber << " ****** " << endl;

    meshesCoarseToFine = GMGSolver::meshesForMultigrid(mesh, polyOrderCoarse, delta_k);
    // truncate meshesCoarseToFine to get a "fair" iteration count measure

    if (truncateMultigridMeshes)
    {
      while (meshesCoarseToFine.size() > numberOfMeshesForMultigrid)
        meshesCoarseToFine.erase(meshesCoarseToFine.begin());
    }

    double l2NormOfIncrement = 1.0;
    int stepNumber = 0;
    while ((l2NormOfIncrement > nlTol) && (stepNumber < nlMaxIters))
    {
      if (!useDirectSolver)
        setGMGSolver(form, meshesCoarseToFine, cgMaxIters, cgTol, useCondensedSolve);
      else
        setDirectSolver(form);

      form.solveAndAccumulate();
      l2NormOfIncrement = form.L2NormSolutionIncrement();
      stepNumber++;

      if (rank==0) cout << stepNumber << ". L^2 norm of increment: " << l2NormOfIncrement;

      if (!useDirectSolver)
      {
        Teuchos::RCP<GMGSolver> gmgSolver = Teuchos::rcp(dynamic_cast<GMGSolver*>(form.getSolver().get()), false);
        int iterationCount = gmgSolver->iterationCount();
        if (rank==0) cout << " (" << iterationCount << " GMG iterations)\n";
      }
      else
      {
        cout << endl;
      }
    }
    
    form.clearSolutionIncrement(); // need to clear before evaluating energy error

    exporter.exportSolution(form.solution(), refNumber);
    // energyErrorExporter.exportFunction({energyErrorFunction}, {"energy error"}, refNumber);
    energyErrorExporter.exportFunction(energyErrorFunction, "energy error", refNumber);

    energyError = form.solutionIncrement()->energyErrorTotal();
    globalDofs = mesh->globalDofCount();
    activeElements = mesh->getTopology()->getActiveCellIndices().size();
    if (rank==0) cout << "Energy error for refinement " << refNumber << ": " << energyError;
    if (rank==0) cout << " (mesh has " << activeElements << " elements and " << globalDofs << " global dofs)." << endl;
  }
  while ((energyError > tol) && (refNumber < numRefs));

  FunctionPtr u1_soln = Function::solution(form.u(1), form.solution());
  double value;
  if (steady)
  {
    value = u1_soln->evaluate(0.5,0.5);
    if (rank==0) cout << "u1(0.5, 0.5) = " << value << endl;
  }
  else
  {
    value = u1_soln->evaluate(0.5,0.5,0.5);
    if (rank==0) cout << "u1(0.5, 0.5) @ t=0.5 = " << value << endl;
  }


  // now solve for the stream function on the fine mesh:
//  if (spaceDim == 2)
//  {
//    form.streamSolution()->solve();
//    HDF5Exporter steadyStreamExporter(form.streamSolution()->mesh(), "navierStokesSteadyCavityFlowStreamSolution", outputDir);
//    steadyStreamExporter.exportSolution(form.streamSolution());
//  }

//  /*   Now that we have a fine mesh, try the same problem, but transient, starting with a zero initial
//   *   state, and with boundary conditions that "ramp up" in time (and which also are zero at time 0).
//   *   We expect to recover the steady solution.
//   */
//
//  double totalTime = 3;
//  double dt = 0.1;
//  int numTimeSteps = ceil(totalTime / dt);
//  StokesVGPFormulation transientForm(spaceDim, useConformingTraces, mu, true, dt);
//
//  FunctionPtr t = transientForm.getTimeFunction();
//  FunctionPtr timeRamp = Teuchos::rcp(new TimeRamp(t,1.0));
//
//  transientForm.initializeSolution(meshTopo, polyOrder, delta_k);
//  transientForm.addZeroMeanPressureCondition();
//  transientForm.addWallCondition(notTopBoundary);
//  transientForm.addInflowCondition(topBoundary, timeRamp * u_topRamp);
//
//  MeshPtr transientMesh = transientForm.solution()->mesh();
//  HDF5Exporter transientExporter(transientMesh, "stokesTransientSolution", ".");
//
//  for (int timeStep=0; timeStep<numTimeSteps; timeStep++) {
//    transientForm.solve();
//    double L2_step = transientForm.L2NormOfTimeStep();
//    transientExporter.exportSolution(transientForm.solution(),transientForm.getTime());
//
//    transientForm.takeTimeStep();
//    if (rank==0) cout << "time step " << timeStep << " completed (L^2 norm of difference from prev: " << L2_step << ").\n";
//  }
//
//  { // trying the same thing as below, but computing it differently:
//    FunctionPtr  p_transient = Function::solution( transientForm.p(), transientForm.solution());
//    FunctionPtr u1_transient = Function::solution(transientForm.u(1), transientForm.solution());
//    FunctionPtr u2_transient = Function::solution(transientForm.u(2), transientForm.solution());
//    FunctionPtr  p_steady = Function::solution( form.p(), form.solution());
//    FunctionPtr u1_steady = Function::solution(form.u(1), form.solution());
//    FunctionPtr u2_steady = Function::solution(form.u(2), form.solution());
//
//    FunctionPtr squaredDiff = (p_transient-p_steady) * (p_transient-p_steady) + (u1_transient-u1_steady) * (u1_transient-u1_steady) + (u2_transient - u2_steady) * (u2_transient - u2_steady);
//    double valSquared = squaredDiff->integrate(form.solution()->mesh());
//    if (rank==0) cout << "L^2 norm of difference between converged transient and steady state solution (computed differently): " << sqrt(valSquared) << endl;
//
//    FunctionPtr p_diff_squared  =   (p_transient-p_steady) *   (p_transient-p_steady);
//    FunctionPtr u1_diff_squared = (u1_transient-u1_steady) * (u1_transient-u1_steady);
//    FunctionPtr u2_diff_squared = (u2_transient-u2_steady) * (u2_transient-u2_steady);
//
//    double p_diff_L2 = sqrt(p_diff_squared->integrate(form.solution()->mesh()));
//    double u1_diff_L2 = sqrt(u1_diff_squared->integrate(form.solution()->mesh()));
//    double u2_diff_L2 = sqrt(u2_diff_squared->integrate(form.solution()->mesh()));
//
//    if (rank==0) cout << "L^2 norm (computed differently) for p: " << p_diff_L2 << endl;
//    if (rank==0) cout << "L^2 norm (computed differently) for u1: " << u1_diff_L2 << endl;
//    if (rank==0) cout << "L^2 norm (computed differently) for u2: " << u2_diff_L2 << endl;
//  }
//
//  // by this point, we should have recovered something close to the steady solution.  Did we?
//  SolutionPtr transientSolution = transientForm.solution();
//  transientSolution->addSolution(form.solution(), -1.0);
//
//  double u1_diff_L2 = sqrt(transientSolution->L2NormOfSolutionGlobal(form.u(1)->ID()));
//  double u2_diff_L2 = sqrt(transientSolution->L2NormOfSolutionGlobal(form.u(2)->ID()));
//  double p_diff_L2 = sqrt(transientSolution->L2NormOfSolutionGlobal(form.p()->ID()));
//
//  double diff_L2 = sqrt(u1_diff_L2 * u1_diff_L2 + u2_diff_L2 * u2_diff_L2 + p_diff_L2 * p_diff_L2);
//
//  if (rank==0) cout << "L^2 norm of difference between converged transient and steady state solution: " << diff_L2 << endl;
//
//  if (rank==0) cout << "L^2 norm for p: " << p_diff_L2 << endl;
//  if (rank==0) cout << "L^2 norm for u1: " << u1_diff_L2 << endl;
//  if (rank==0) cout << "L^2 norm for u2: " << u2_diff_L2 << endl;

  return 0;
}
