//
//  GMGOperatorTests.cpp
//  Camellia
//
//  Created by Nate Roberts on 2/4/15.
//
//

#include "EpetraExt_RowMatrixOut.h"
#include "Teuchos_UnitTestHarness.hpp"

#include "CamelliaDebugUtility.h"
#include "CamelliaTestingHelpers.h"
#include "GDAMinimumRule.h"
#include "GMGOperator.h"
#include "MeshFactory.h"
#include "PoissonFormulation.h"
#include "RHS.h"
#include "StokesVGPFormulation.h"

using namespace Camellia;
using namespace Intrepid;

namespace
{
  enum FormulationChoice {
    Poisson, Stokes
  };
  
  void testEpetraMatrixIsIdentity(Teuchos::RCP<Epetra_CrsMatrix> P, Teuchos::FancyOStream &out, bool &success)
  {
    int myRowCount = P->Map().NumMyElements();
    
    GlobalIndexTypeToCast globalEntryCount = P->Map().NumGlobalElements();
    Intrepid::FieldContainer<GlobalIndexTypeToCast> colIndices(globalEntryCount);
    Intrepid::FieldContainer<double> colValues(globalEntryCount);
    
//    printMapSummary(P->Map(), "P->Map()");
    
    for (int localRowIndex=0; localRowIndex<myRowCount; localRowIndex++)
    {
      GlobalIndexTypeToCast globalRowIndex = P->Map().GID(localRowIndex);
      int numEntries;
      P->ExtractMyRowCopy(localRowIndex, globalEntryCount, numEntries, &colValues(0), &colIndices(0));
      bool diagEntryFound = false;
      
      double tol=1e-14;
      for (int colEntryOrdinal=0; colEntryOrdinal<numEntries; colEntryOrdinal++)
      {
        GlobalIndexTypeToCast localColIndex = colIndices(colEntryOrdinal);
        GlobalIndexTypeToCast globalColIndex = P->DomainMap().GID(localColIndex);
        double expectedValue;
        double actualValue = colValues(colEntryOrdinal);
        if (globalColIndex != globalRowIndex)
        {
          // expect 0 for off-diagonals
          TEST_COMPARE(abs(actualValue), <, tol);
        }
        else
        {
          // expect 1 on the diagonal
          expectedValue = 1.0;
          TEST_FLOATING_EQUALITY(expectedValue, actualValue, tol);
          diagEntryFound = true;
        }
      }
      if (!diagEntryFound)
      {
        int rank = Teuchos::GlobalMPISession::getRank();
        cout << "on rank " << rank << ", no diagonal entry found for global row " << globalRowIndex;
        cout << " (num col entries: " << numEntries << ")\n";
      }
      TEST_ASSERT(diagEntryFound);
    }
    
//    { // DEBUGGING: export to disk
//      EpetraExt::RowMatrixToMatrixMarketFile("/tmp/P.dat",*P,NULL,NULL,false);
//      int rank = Teuchos::GlobalMPISession::getRank();
//      if (rank==0) cout << "wrote prolongation operator matrix to /tmp/P.dat\n";
//    }
  }

  TEUCHOS_UNIT_TEST( GMGOperator, IdentityProlongationOperatorHangingNode_2D)
  {
    // take a mesh with a hanging node, using the same mesh for coarse and fine in a GMGOperator.
    // test that the prolongation operator is the identity
    int spaceDim = 2;
    bool useConformingTraces = true;
    int H1Order = useConformingTraces ? 1 : 2; // make trace variables linear
    bool useStaticCondensation = false;
    vector<int> cellCounts = {1,2}; // two elements
    PoissonFormulation form(spaceDim, useConformingTraces);
    BFPtr bf = form.bf();
    
    int delta_k = spaceDim;
    vector<double> dimensions(spaceDim,1.0);
    MeshPtr mesh = MeshFactory::rectilinearMesh(bf, dimensions, cellCounts, H1Order, delta_k);
    
    // refine element 0:
    set<GlobalIndexType> cellsToRefine = {0};
    mesh->hRefine(cellsToRefine);
    
    BCPtr bc = BC::bc();
    IPPtr ip = bf->graphNorm();
    RHSPtr rhs = RHS::rhs();
    
    SolutionPtr fineSoln = Solution::solution(mesh);
    fineSoln->setIP(ip);
    fineSoln->setRHS(rhs);
    fineSoln->setBC(bc);
    fineSoln->setUseCondensedSolve(useStaticCondensation);
    
    SolverPtr coarseSolver = Solver::getSolver(Solver::KLU, true);
    
    GMGOperator gmgOperator(bc,mesh,ip,mesh,fineSoln->getDofInterpreter(),
                            fineSoln->getPartitionMap(),coarseSolver, useStaticCondensation);
    gmgOperator.constructProlongationOperator();
    
    Teuchos::RCP<Epetra_CrsMatrix> P = gmgOperator.getProlongationOperator();
    testEpetraMatrixIsIdentity(P, out, success);
  }
  
  void testIdentityProlongationOperatorUniform(FormulationChoice formChoice, int spaceDim,
                                               bool useConformingTraces, bool useStaticCondensation,
                                               Teuchos::FancyOStream &out, bool &success)
  {
    // take a uniform mesh, using the same mesh for coarse and fine in a GMGOperator.
    // test that the prolongation operator is the identity
//    int H1Order = useConformingTraces ? 1 : 2; // make trace variables linear
    int H1Order = 1;

    vector<int> cellCounts(spaceDim,2);
    BFPtr bf;
    BCPtr bc = BC::bc();
    MeshPtr mesh;
    int delta_k = spaceDim;
    vector<double> dimensions(spaceDim,1.0);

    if (formChoice == Poisson)
    {
      PoissonFormulation form(spaceDim, useConformingTraces);
      bf = form.bf();
      mesh = MeshFactory::rectilinearMesh(bf, dimensions, cellCounts, H1Order, delta_k);
    }
    else
    {
      double mu = 1.0;
      StokesVGPFormulation form = StokesVGPFormulation::steadyFormulation(spaceDim,mu,useConformingTraces);
      bf = form.bf();
      mesh = MeshFactory::rectilinearMesh(bf, dimensions, cellCounts, H1Order, delta_k);
      bc->addSinglePointBC(form.p()->ID(), 0.0, mesh);
    }
    
    IPPtr ip = bf->graphNorm();
    RHSPtr rhs = RHS::rhs();
    
    SolutionPtr fineSoln = Solution::solution(mesh);
    fineSoln->setBC(bc);
    fineSoln->setIP(ip);
    fineSoln->setRHS(rhs);
    fineSoln->setUseCondensedSolve(useStaticCondensation);
    
    SolverPtr coarseSolver = Solver::getSolver(Solver::KLU, true);
    
    GMGOperator gmgOperator(bc,mesh,ip,mesh,fineSoln->getDofInterpreter(),
                            fineSoln->getPartitionMap(),coarseSolver, useStaticCondensation);
    gmgOperator.constructProlongationOperator();
    
    Teuchos::RCP<Epetra_CrsMatrix> P = gmgOperator.getProlongationOperator();
    testEpetraMatrixIsIdentity(P, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, IdentityProlongationOperatorUniform_2D )
  {
    int spaceDim = 2;
    bool useConformingTraces = true;
    bool useStaticCondensation = false;

    testIdentityProlongationOperatorUniform(Poisson, spaceDim, useConformingTraces, useStaticCondensation, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, IdentityProlongationOperatorUniform_StokesCondensed2D )
  {
    int spaceDim = 2;
    bool useConformingTraces = false;
    bool useStaticCondensation = true;
    
    testIdentityProlongationOperatorUniform(Stokes, spaceDim, useConformingTraces, useStaticCondensation, out, success);
  }
  
  void testProlongationOperatorLine(bool useStaticCondensation, Teuchos::FancyOStream &out, bool &success)
  {
    /*
     
     Take a 1D, 1-element mesh with an exact solution.
     Refine.
     Compute the GMGOperator's prolongation of the coarse exact solution onto the fine mesh.
     Check that the solution on the fine mesh is exact.
     
     */
    
    int spaceDim = 1;
    bool useConformingTraces = false;
    int H1Order = 2, delta_k = spaceDim;
    PoissonFormulation form(spaceDim, useConformingTraces);
    BFPtr bf = form.bf();
    VarPtr phi = form.phi(), psi = form.psi(), phi_hat = form.phi_hat(), psi_n_hat = form.psi_n_hat();
    FunctionPtr phi_exact;
    if (H1Order >= 3)
      phi_exact = Function::xn(2); // x^2 exact solution
    else
      phi_exact = Function::constant(2); // constant exact solution
    FunctionPtr psi_exact = phi_exact->dx();
    
    double xLeft = 0.0, xRight = 1.0;
    int coarseElementCount = 1;

    MeshPtr mesh = MeshFactory::intervalMesh(bf, xLeft, xRight, coarseElementCount, H1Order, delta_k);
    // for debugging, do a refinement first:
    //  mesh->hRefine(mesh->getActiveCellIDs());
    
    SolutionPtr coarseSoln = Solution::solution(mesh);
    
    BCPtr bc = BC::bc();
    bc->addDirichlet(phi_hat, SpatialFilter::allSpace(), phi_exact);
    VarPtr q = form.q();
    RHSPtr rhs = RHS::rhs();
    rhs->addTerm(phi_exact->dx()->dx() * q);
    coarseSoln->setRHS(rhs);
    coarseSoln->setBC(bc);
    coarseSoln->setUseCondensedSolve(useStaticCondensation);
    
    IPPtr ip = bf->graphNorm();
    coarseSoln->setIP(ip);
    
    FunctionPtr n = Function::normal_1D();
    FunctionPtr parity = Function::sideParity();
    
    map<int, FunctionPtr> exactSolnMap;
    exactSolnMap[phi->ID()] = phi_exact;
    exactSolnMap[psi->ID()] = psi_exact;
    exactSolnMap[phi_hat->ID()] = phi_exact * n * parity;
    exactSolnMap[psi_n_hat->ID()] = psi_exact * n * parity;
    
    coarseSoln->projectOntoMesh(exactSolnMap);
    
    { // DEBUGGING
      bool warnAboutOffRank = false;
      set<GlobalIndexType> myCellIDs = coarseSoln->mesh()->cellIDsInPartition();
      for (GlobalIndexType cellID : myCellIDs) {
        FieldContainer<double> coefficients = coarseSoln->allCoefficientsForCellID(cellID, warnAboutOffRank);
        out << "\n\n******************** Dofs for cell " << cellID << " (exactSoln) ********************\n\n";
        DofOrderingPtr trialOrder = mesh->getElementType(cellID)->trialOrderPtr;
        printLabeledDofCoefficients(out, form.bf()->varFactory(), trialOrder, coefficients);
      }
    }
    
    double energyError = coarseSoln->energyErrorTotal();
    
    // sanity check: our exact solution should give us 0 energy error
    double tol = 1e-13;
    TEST_COMPARE(energyError, <, tol);
    if (energyError > tol)
    {
      map<GlobalIndexType, double> errorForMyCells = coarseSoln->rankLocalEnergyError();
      for (auto entry : errorForMyCells)
      {
        out << "Error for cell " << entry.first << ": " << entry.second << endl;
      }
      out << *coarseSoln->getLHSVector();
    }
    
    MeshPtr fineMesh = mesh->deepCopy();
    
    fineMesh->hRefine(fineMesh->getActiveCellIDs());
    //  fineMesh->hRefine(fineMesh->getActiveCellIDs());
    
    SolutionPtr fineSoln = Solution::solution(fineMesh);
    fineSoln->setBC(bc);
    fineSoln->setIP(ip);
    fineSoln->setRHS(rhs);
    fineSoln->setUseCondensedSolve(useStaticCondensation);
    
    // again, a sanity check, now on the fine solution:
    fineSoln->projectOntoMesh(exactSolnMap);
    bool warnAboutOffRank = false;
    set<GlobalIndexType> myCellIDs = fineSoln->mesh()->cellIDsInPartition();
    for (GlobalIndexType cellID : myCellIDs) {
      FieldContainer<double> coefficients = fineSoln->allCoefficientsForCellID(cellID, warnAboutOffRank);
      out << "\n\n******************** Dofs for cell " << cellID << " (exactSoln) ********************\n\n";
      DofOrderingPtr trialOrder = fineMesh->getElementType(cellID)->trialOrderPtr;
      printLabeledDofCoefficients(out, form.bf()->varFactory(), trialOrder, coefficients);
    }
    
    energyError = fineSoln->energyErrorTotal();
    TEST_COMPARE(energyError, <, tol);
    
    
    SolverPtr coarseSolver = Solver::getSolver(Solver::KLU, true);
    GMGOperator gmgOperator(bc,mesh,ip,fineMesh,fineSoln->getDofInterpreter(),
                            fineSoln->getPartitionMap(),coarseSolver, useStaticCondensation);
    
    Teuchos::RCP<Epetra_CrsMatrix> P = gmgOperator.constructProlongationOperator();
    Teuchos::RCP<Epetra_FEVector> coarseSolutionVector = coarseSoln->getLHSVector();
    
    fineSoln->initializeLHSVector();
    fineSoln->getLHSVector()->PutScalar(0); // set a 0 global solution
    
    fineSoln->importSolution();      // imports the 0 solution onto each cell
    fineSoln->clearComputedResiduals();
    
    P->Multiply(false, *coarseSolutionVector, *fineSoln->getLHSVector());
    
//    cout << "coarseSolutionVector:\n"  << *coarseSolutionVector;
//    cout << "P * coarseSolutionVector:\n"  << *fineSoln->getLHSVector();
    
    fineSoln->importSolution();
    
    energyError = fineSoln->energyErrorTotal();
    TEST_COMPARE(energyError, <, tol);
    
    for (GlobalIndexType cellID : myCellIDs) {
      FieldContainer<double> coefficients = fineSoln->allCoefficientsForCellID(cellID, warnAboutOffRank);
      out << "\n\n******************** Dofs for cell " << cellID << " (fineSoln) ********************\n\n";
      DofOrderingPtr trialOrder = fineMesh->getElementType(cellID)->trialOrderPtr;
      printLabeledDofCoefficients(out, form.bf()->varFactory(), trialOrder, coefficients);
    }

//    { // DEBUGGING: export to disk
//      EpetraExt::RowMatrixToMatrixMarketFile("/tmp/P.dat",*P,NULL,NULL,false);
//      int rank = Teuchos::GlobalMPISession::getRank();
//      if (rank==0) cout << "wrote prolongation operator matrix to /tmp/P.dat\n";
//      
//      GDAMinimumRule* fineGDA = dynamic_cast<GDAMinimumRule*>(fineMesh->globalDofAssignment().get());
//      fineGDA->printGlobalDofInfo();
//      
//      GDAMinimumRule* coarseGDA = dynamic_cast<GDAMinimumRule*>(mesh->globalDofAssignment().get());
//      coarseGDA->printGlobalDofInfo();
//    }

  }

  void testProlongationOperatorQuad(bool simple, bool useStaticCondensation, bool useConformingTraces,
                                    bool testHMultigrid, int levels, bool uniform,
                                    Teuchos::FancyOStream &out, bool &success)
  {
    int spaceDim = 2;
    PoissonFormulation form(spaceDim, useConformingTraces);
    BFPtr bf = form.bf();
    VarPtr phi = form.phi(), psi = form.psi(), phi_hat = form.phi_hat(), psi_n_hat = form.psi_n_hat();
    
    int H1Order, delta_k = 1;
    
    map<int,int> trialOrderEnhancements;
    
    FunctionPtr phi_exact, psi_exact;
    if (simple)
    {
      H1Order = 1;
      phi_exact = Function::constant(3.14159);
      FunctionPtr zero = Function::zero();
      psi_exact = Function::vectorize(zero, zero);
    }
    else
    {
      H1Order = 3;
      phi_exact = Function::xn(2) + Function::yn(1);
      psi_exact = phi_exact->grad();
    }
    
//    if (useConformingTraces)
//    {
//      trialOrderEnhancements[phi->ID()] = 1;
//    }
    
    int coarseElementCount = testHMultigrid ? 1 : 2;
    vector<double> dimensions(spaceDim,1.0);
    vector<int> elementCounts(spaceDim,coarseElementCount);
    MeshPtr mesh = MeshFactory::rectilinearMesh(bf, dimensions, elementCounts, H1Order, delta_k, {0,0}, trialOrderEnhancements);
    
    SolutionPtr coarseSoln = Solution::solution(mesh);
    BCPtr bc = BC::bc();
    coarseSoln->setBC(bc);
    coarseSoln->setUseCondensedSolve(useStaticCondensation);
    
    VarPtr q = form.q();
    RHSPtr rhs = RHS::rhs();
    FunctionPtr f = phi_exact->dx()->dx() + phi_exact->dy()->dy();
    rhs->addTerm(f * q);
    coarseSoln->setRHS(rhs);
    IPPtr ip = bf->graphNorm();
    coarseSoln->setIP(ip);
    coarseSoln->setUseCondensedSolve(useStaticCondensation);
    
    map<int, FunctionPtr> exactSolnMap;
    exactSolnMap[phi->ID()] = phi_exact;
    exactSolnMap[psi->ID()] = psi_exact;
    
    FunctionPtr phi_hat_exact   =   phi_hat->termTraced()->evaluate(exactSolnMap);
    FunctionPtr psi_n_hat_exact = psi_n_hat->termTraced()->evaluate(exactSolnMap);
    
    exactSolnMap[phi_hat->ID()]   = phi_hat_exact;
    exactSolnMap[psi_n_hat->ID()] = psi_n_hat_exact;
    
    coarseSoln->projectOntoMesh(exactSolnMap);
    
    double energyError = coarseSoln->energyErrorTotal();
    
    // sanity check: our exact solution should give us 0 energy error
    double tol = 1e-12;
    TEST_COMPARE(energyError, <, tol);
    
    MeshPtr fineMesh = mesh->deepCopy();
    GlobalIndexType lastRefinedCellID = -1;
    for (int i=0; i<levels; i++)
    {
      set<GlobalIndexType> cellsToRefine;
      if (uniform)
      {
        cellsToRefine = fineMesh->getActiveCellIDs();
      }
      else
      {
        // refine the one we last refined (in p case), or its child (h case)
        if (lastRefinedCellID == -1)
        {
          // then refine the first active guy:
          lastRefinedCellID = *fineMesh->getActiveCellIDs().begin();
        }
        else if (testHMultigrid)
        {
          CellPtr lastRefinedCell = fineMesh->getTopology()->getCell(lastRefinedCellID);
          lastRefinedCellID = lastRefinedCell->getChildIndices(fineMesh->getTopology())[0];
        }
        cellsToRefine.insert(lastRefinedCellID);
      }
      
      if (testHMultigrid)
      {
        fineMesh->hRefine(cellsToRefine);
        fineMesh->enforceOneIrregularity();
      }
      else
        fineMesh->pRefine(cellsToRefine);
    }
    
    SolutionPtr exactSoln = Solution::solution(fineMesh);
    exactSoln->setIP(ip);
    exactSoln->setRHS(rhs);
    
    // again, a sanity check, now on the exact fine solution:
    exactSoln->projectOntoMesh(exactSolnMap);
    energyError = exactSoln->energyErrorTotal();
    TEST_COMPARE(energyError, <, tol);
    
    SolutionPtr fineSoln = Solution::solution(fineMesh);
    fineSoln->setBC(bc);
    fineSoln->setIP(ip);
    fineSoln->setRHS(rhs);
    fineSoln->setUseCondensedSolve(useStaticCondensation);
    
    SolverPtr coarseSolver = Solver::getSolver(Solver::KLU, true);
    GMGOperator gmgOperator(bc,mesh,ip,fineMesh,fineSoln->getDofInterpreter(),
                            fineSoln->getPartitionMap(),coarseSolver, useStaticCondensation);
    
    Teuchos::RCP<Epetra_CrsMatrix> P = gmgOperator.constructProlongationOperator();
    Teuchos::RCP<Epetra_FEVector> coarseSolutionVector = coarseSoln->getLHSVector();
    
    fineSoln->initializeLHSVector();
    fineSoln->getLHSVector()->PutScalar(0); // set a 0 global solution
    fineSoln->importSolution();      // imports the 0 solution onto each cell
    fineSoln->clearComputedResiduals();
    
    P->Multiply(false, *coarseSolutionVector, *fineSoln->getLHSVector());
    
    fineSoln->importSolution();
    
    set<GlobalIndexType> cellIDs = fineSoln->mesh()->getActiveCellIDs();
    
    energyError = fineSoln->energyErrorTotal();
    TEST_COMPARE(energyError, <, tol);
    
    bool warnAboutOffRank = false;
    VarFactoryPtr vf = bf->varFactory();
    //    for (set<GlobalIndexType>::iterator cellIDIt = cellIDs.begin(); cellIDIt != cellIDs.end(); cellIDIt++) {
    //      cout << "\n\n******************** Dofs for cell " << *cellIDIt << " (fineSoln before subtracting exact) ********************\n";
    //      FieldContainer<double> coefficients = fineSoln->allCoefficientsForCellID(*cellIDIt, warnAboutOffRank);
    //      DofOrderingPtr trialOrder = fineMesh->getElementType(*cellIDIt)->trialOrderPtr;
    //      printLabeledDofCoefficients(vf, trialOrder, coefficients);
    //    }
    
    set<GlobalIndexType> myCellIDs = fineSoln->mesh()->cellIDsInPartition();
    
    for (GlobalIndexType cellID : myCellIDs) {
      FieldContainer<double> coefficients = exactSoln->allCoefficientsForCellID(cellID, warnAboutOffRank);
      out << "\n\n******************** Dofs for cell " << cellID << " (exactSoln) ********************\n\n";
      DofOrderingPtr trialOrder = fineMesh->getElementType(cellID)->trialOrderPtr;
      printLabeledDofCoefficients(out, form.bf()->varFactory(), trialOrder, coefficients);
    }
    
    for (GlobalIndexType cellID : myCellIDs) {
      FieldContainer<double> coefficients = fineSoln->allCoefficientsForCellID(cellID, warnAboutOffRank);
      out << "\n\n******************** Dofs for cell " << cellID << " (fineSoln) ********************\n\n";
      DofOrderingPtr trialOrder = fineMesh->getElementType(cellID)->trialOrderPtr;
      printLabeledDofCoefficients(out, form.bf()->varFactory(), trialOrder, coefficients);
    }
    
    fineSoln->addSolution(exactSoln, -1.0); // should recover zero solution this way
    
    // import global solution data onto each rank:
    fineSoln->importSolutionForOffRankCells(cellIDs);
    
    for (GlobalIndexType cellID : cellIDs) {
      FieldContainer<double> coefficients = fineSoln->allCoefficientsForCellID(cellID, warnAboutOffRank);
      FieldContainer<double> expectedCoefficients(coefficients.size()); // zero coefficients
      
      TEST_COMPARE_FLOATING_ARRAYS_CAMELLIA_ABSTOLTOO(coefficients, expectedCoefficients, tol, tol);
      //    cout << "\n\n******************** Dofs for cell " << cellID << " (fineSoln after subtracting exact) ********************\n\n";
      //    DofOrderingPtr trialOrder = fineMesh->getElementType(cellID)->trialOrderPtr;
      //    printLabeledDofCoefficients(form.bf()->varFactory(), trialOrder, coefficients);
    }
  }

  void testQuadFineCoarseSwap(bool simple, bool useStaticCondensation, Teuchos::FancyOStream &out, bool &success)
  {
    int spaceDim = 2;
    PoissonFormulation formConforming(spaceDim, true);
    PoissonFormulation formNonconforming(spaceDim, false);
    BFPtr bfConforming = formConforming.bf();
    BFPtr bfNonconforming = formNonconforming.bf();
    VarPtr phi = formConforming.phi(), psi = formConforming.psi(), phi_hat = formConforming.phi_hat(), psi_n_hat = formConforming.psi_n_hat();
    
    int H1Order, delta_k = 1;
    
    FunctionPtr phi_exact, psi_exact;
    if (simple)
    {
      H1Order = 1;
      phi_exact = Function::constant(3.14159);
      FunctionPtr zero = Function::zero();
      psi_exact = Function::vectorize(zero, zero);
    }
    else
    {
      H1Order = 3;
      phi_exact = Function::xn(2) + Function::yn(1);
      psi_exact = phi_exact->grad();
    }
    
    int coarseElementCount = 1;
    vector<double> dimensions(2,1.0);
    vector<int> elementCounts(2,coarseElementCount);
    MeshPtr meshConforming = MeshFactory::rectilinearMesh(bfConforming, dimensions, elementCounts, H1Order, delta_k);
    map<int,int> trialEnhancements;
    trialEnhancements[phi_hat->ID()] = 1;
    MeshPtr meshNonconforming = Teuchos::rcp( new Mesh(meshConforming->getTopology(), bfNonconforming, H1Order, delta_k, trialEnhancements) );
    
    SolutionPtr coarseSoln = Solution::solution(meshNonconforming);
    BCPtr bc = BC::bc();
    coarseSoln->setBC(bc);
    coarseSoln->setUseCondensedSolve(useStaticCondensation);
    
    VarPtr q = formConforming.q();
    RHSPtr rhs = RHS::rhs();
    FunctionPtr f = phi_exact->dx()->dx() + phi_exact->dy()->dy();
    rhs->addTerm(f * q);
    coarseSoln->setRHS(rhs);
    IPPtr ip = bfConforming->graphNorm();
    coarseSoln->setIP(ip);
    coarseSoln->setUseCondensedSolve(useStaticCondensation);
    
    map<int, FunctionPtr> exactSolnMap;
    exactSolnMap[phi->ID()] = phi_exact;
    exactSolnMap[psi->ID()] = psi_exact;
    
    FunctionPtr phi_hat_exact   =   phi_hat->termTraced()->evaluate(exactSolnMap);
    FunctionPtr psi_n_hat_exact = psi_n_hat->termTraced()->evaluate(exactSolnMap);
    
    exactSolnMap[phi_hat->ID()]   = phi_hat_exact;
    exactSolnMap[psi_n_hat->ID()] = psi_n_hat_exact;
    
    coarseSoln->projectOntoMesh(exactSolnMap);
    
    double energyError = coarseSoln->energyErrorTotal();
    
    // sanity check: our exact solution should give us 0 energy error
    double tol = 1e-12;
    TEST_COMPARE(energyError, <, tol);
    
    MeshPtr fineMesh = meshConforming;
    MeshPtr coarseMesh = meshNonconforming;
    
    SolutionPtr fineSoln = Solution::solution(fineMesh);
    fineSoln->setBC(bc);
    fineSoln->setIP(ip);
    fineSoln->setRHS(rhs);
    fineSoln->setUseCondensedSolve(useStaticCondensation);
    
    SolverPtr coarseSolver = Solver::getSolver(Solver::KLU, true);
    GMGOperator gmgOperator(bc,meshNonconforming,ip,fineMesh,fineSoln->getDofInterpreter(),
                            fineSoln->getPartitionMap(),coarseSolver, useStaticCondensation);
    gmgOperator.setFineCoarseRolesSwapped(true);
    
    Teuchos::RCP<Epetra_CrsMatrix> P = gmgOperator.constructProlongationOperator();
    Teuchos::RCP<Epetra_FEVector> coarseSolutionVector = coarseSoln->getLHSVector();
    
    cout << "Outputting P to file.\n";
    EpetraExt::RowMatrixToMatrixMarketFile("/tmp/P.dat",*P, NULL, NULL, false); // false: don't write header
    
    fineSoln->projectOntoMesh(exactSolnMap);
    coarseSoln->initializeLHSVector();
    coarseSoln->getLHSVector()->PutScalar(0); // set a 0 global solution
    coarseSoln->importSolution();             // imports the 0 solution onto each cell
    coarseSoln->clearComputedResiduals();
    
    P->Multiply(true, *fineSoln->getLHSVector(), *coarseSolutionVector); // true: transpose
    
    coarseSoln->importSolution();
    
    set<GlobalIndexType> cellIDs = coarseSoln->mesh()->getActiveCellIDs();
    
    energyError = coarseSoln->energyErrorTotal();
    TEST_COMPARE(energyError, <, tol);
    
    bool warnAboutOffRank = false;
//    VarFactoryPtr vf = bf->varFactory();
    //    for (set<GlobalIndexType>::iterator cellIDIt = cellIDs.begin(); cellIDIt != cellIDs.end(); cellIDIt++) {
    //      cout << "\n\n******************** Dofs for cell " << *cellIDIt << " (fineSoln before subtracting exact) ********************\n";
    //      FieldContainer<double> coefficients = fineSoln->allCoefficientsForCellID(*cellIDIt, warnAboutOffRank);
    //      DofOrderingPtr trialOrder = fineMesh->getElementType(*cellIDIt)->trialOrderPtr;
    //      printLabeledDofCoefficients(vf, trialOrder, coefficients);
    //    }
    
    set<GlobalIndexType> myCellIDs = fineSoln->mesh()->cellIDsInPartition();
    
    for (GlobalIndexType cellID : myCellIDs) {
      FieldContainer<double> coefficients = coarseSoln->allCoefficientsForCellID(cellID, warnAboutOffRank);
      out << "\n\n******************** Dofs for cell " << cellID << " (coarseSoln) ********************\n\n";
      DofOrderingPtr trialOrder = coarseMesh->getElementType(cellID)->trialOrderPtr;
      printLabeledDofCoefficients(out, formConforming.bf()->varFactory(), trialOrder, coefficients);
    }
    
    SolutionPtr exactSoln = Solution::solution(coarseMesh);
    exactSoln->setIP(ip);
    exactSoln->setRHS(rhs);
    
    // again, a sanity check, now on the exact fine solution:
    exactSoln->projectOntoMesh(exactSolnMap);
    energyError = exactSoln->energyErrorTotal();
    TEST_COMPARE(energyError, <, tol);
    
    for (GlobalIndexType cellID : myCellIDs) {
      FieldContainer<double> coefficients = exactSoln->allCoefficientsForCellID(cellID, warnAboutOffRank);
      out << "\n\n******************** Dofs for cell " << cellID << " (exactSoln) ********************\n\n";
      DofOrderingPtr trialOrder = coarseMesh->getElementType(cellID)->trialOrderPtr;
      printLabeledDofCoefficients(out, formConforming.bf()->varFactory(), trialOrder, coefficients);
    }
    
    coarseSoln->addSolution(exactSoln, -1.0); // should recover zero solution this way
    
    // import global solution data onto each rank:
    coarseSoln->importSolutionForOffRankCells(cellIDs);
    
    for (GlobalIndexType cellID : cellIDs) {
      FieldContainer<double> coefficients = coarseSoln->allCoefficientsForCellID(cellID, warnAboutOffRank);
      FieldContainer<double> expectedCoefficients(coefficients.size()); // zero coefficients
      
      TEST_COMPARE_FLOATING_ARRAYS_CAMELLIA_ABSTOLTOO(coefficients, expectedCoefficients, tol, tol);
      //    cout << "\n\n******************** Dofs for cell " << cellID << " (fineSoln after subtracting exact) ********************\n\n";
      //    DofOrderingPtr trialOrder = fineMesh->getElementType(cellID)->trialOrderPtr;
      //    printLabeledDofCoefficients(form.bf()->varFactory(), trialOrder, coefficients);
    }
  }

  
  // Commenting out the FineCoarseSwap test because we likely won't need this feature
  // TODO: remove the feature from GMGOperator.
  /*TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_FineCoarseSwap )
  {
    bool simple = true;
    bool useStaticCondensation = false;
    testQuadFineCoarseSwap(simple, useStaticCondensation, out, success);
  }*/
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorLineStandardSolve )
  {
    bool useStaticCondensation = false;
    testProlongationOperatorLine(useStaticCondensation, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorLineCondensedSolve )
  {
    MPIWrapper::CommWorld()->Barrier();
    
    bool useStaticCondensation = true;
    testProlongationOperatorLine(useStaticCondensation, out, success);
  }

  //**** p-Multigrid prolongation tests on the quad *****//
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SimpleCondensedSolveNonconforming_p )
  {
    bool simple = true;
    bool useStaticCondensation = true;
    bool useConformingTraces = false;
    bool testHMultigrid = false;
    int levels = 1;
    bool uniform = true;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SimpleCondensedSolveConforming_p )
  {
    bool simple = true;
    bool useStaticCondensation = true;
    bool useConformingTraces = true;
    bool testHMultigrid = false;
    int levels = 1;
    bool uniform = true;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SimpleStandardSolveNonconforming_p )
  {
    bool simple = true;
    bool useStaticCondensation = false;
    bool useConformingTraces = false;
    bool testHMultigrid = false;
    int levels = 1;
    bool uniform = true;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SimpleStandardSolveConforming_p )
  {
    bool simple = true;
    bool useStaticCondensation = false;
    bool useConformingTraces = true;
    bool testHMultigrid = false;
    int levels = 1;
    bool uniform = true;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SlowCondensedSolveNonconforming_p )
  {
    bool simple = false;
    bool useStaticCondensation = true;
    bool useConformingTraces = false;
    bool testHMultigrid = false;
    int levels = 1;
    bool uniform = true;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SlowCondensedSolveConforming_p )
  {
    bool simple = false;
    bool useStaticCondensation = true;
    bool useConformingTraces = true;
    bool testHMultigrid = false;
    int levels = 1;
    bool uniform = true;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SlowStandardSolveNonconforming_p )
  {
    bool simple = false;
    bool useStaticCondensation = false;
    bool useConformingTraces = false;
    bool testHMultigrid = false;
    int levels = 1;
    bool uniform = true;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SlowStandardSolveConforming_p )
  {
    bool simple = false;
    bool useStaticCondensation = false;
    bool useConformingTraces = true;
    bool testHMultigrid = false;
    int levels = 1;
    bool uniform = true;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  //**** h-Multigrid tests below *****//
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SimpleCondensedSolveNonconforming )
  {
    bool simple = true;
    bool useStaticCondensation = true;
    bool useConformingTraces = false;
    bool testHMultigrid = true;
    int levels = 1;
    bool uniform = true;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SimpleCondensedSolveConforming )
  {
    bool simple = true;
    bool useStaticCondensation = true;
    bool useConformingTraces = true;
    bool testHMultigrid = true;
    int levels = 1;
    bool uniform = true;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SimpleStandardSolveNonconforming )
  {
    bool simple = true;
    bool useStaticCondensation = false;
    bool useConformingTraces = false;
    bool testHMultigrid = true;
    int levels = 1;
    bool uniform = true;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SimpleStandardSolveConforming )
  {
    bool simple = true;
    bool useStaticCondensation = false;
    bool useConformingTraces = true;
    bool testHMultigrid = true;
    int levels = 1;
    bool uniform = true;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SlowCondensedSolveNonconforming )
  {
    bool simple = false;
    bool useStaticCondensation = true;
    bool useConformingTraces = false;
    bool testHMultigrid = true;
    int levels = 1;
    bool uniform = true;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SlowCondensedSolveConforming )
  {
    bool simple = false;
    bool useStaticCondensation = true;
    bool useConformingTraces = true;
    bool testHMultigrid = true;
    int levels = 1;
    bool uniform = true;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SlowStandardSolveNonconforming )
  {
    bool simple = false;
    bool useStaticCondensation = false;
    bool useConformingTraces = false;
    bool testHMultigrid = true;
    int levels = 1;
    bool uniform = true;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SlowStandardSolveConforming )
  {
    bool simple = false;
    bool useStaticCondensation = false;
    bool useConformingTraces = true;
    bool testHMultigrid = true;
    int levels = 1;
    bool uniform = true;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  /***
   
   Commenting out the two-level tests because, although these fail, this is not a use case currently
   supported by the code, and it's not right now a priority to add it.

  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SimpleCondensedSolveTwoLevelNonconforming )
  {
    bool simple = true;
    bool useStaticCondensation = true;
    bool useConformingTraces = false;
    bool testHMultigrid = true;
    int levels = 2;
    bool uniform = true;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SimpleCondensedSolveTwoLevelHangingNodesNonconforming )
  {
    bool simple = true;
    bool useStaticCondensation = true;
    bool useConformingTraces = false;
    bool testHMultigrid = true;
    int levels = 2;
    bool uniform = false;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SimpleCondensedSolveTwoLevelConforming )
  {
    bool simple = true;
    bool useStaticCondensation = true;
    bool useConformingTraces = true;
    bool testHMultigrid = true;
    int levels = 2;
    bool uniform = true;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SimpleCondensedSolveTwoLevelHangingNodesConforming )
  {
    bool simple = true;
    bool useStaticCondensation = true;
    bool useConformingTraces = true;
    bool testHMultigrid = true;
    int levels = 2;
    bool uniform = false;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SimpleStandardSolveTwoLevelNonconforming )
  {
    bool simple = true;
    bool useStaticCondensation = false;
    bool useConformingTraces = false;
    bool testHMultigrid = true;
    int levels = 2;
    bool uniform = true;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SimpleStandardSolveTwoLevelHangingNodesNonconforming )
  {
    bool simple = true;
    bool useStaticCondensation = false;
    bool useConformingTraces = false;
    bool testHMultigrid = true;
    int levels = 2;
    bool uniform = false;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SimpleStandardSolveTwoLevelConforming )
  {
    bool simple = true;
    bool useStaticCondensation = false;
    bool useConformingTraces = true;
    bool testHMultigrid = true;
    int levels = 2;
    bool uniform = true;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SimpleStandardSolveTwoLevelHangingNodesConforming )
  {
    bool simple = true;
    bool useStaticCondensation = false;
    bool useConformingTraces = true;
    bool testHMultigrid = true;
    int levels = 2;
    bool uniform = false;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SlowCondensedSolveTwoLevelNonconforming )
  {
    bool simple = false;
    bool useStaticCondensation = true;
    bool useConformingTraces = false;
    bool testHMultigrid = true;
    int levels = 2;
    bool uniform = true;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SlowCondensedSolveTwoLevelHangingNodesNonconforming )
  {
    bool simple = false;
    bool useStaticCondensation = true;
    bool useConformingTraces = false;
    bool testHMultigrid = true;
    int levels = 2;
    bool uniform = false;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SlowCondensedSolveTwoLevelConforming )
  {
    bool simple = false;
    bool useStaticCondensation = true;
    bool useConformingTraces = true;
    bool testHMultigrid = true;
    int levels = 2;
    bool uniform = true;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SlowCondensedSolveTwoLevelHangingNodesConforming )
  {
    bool simple = false;
    bool useStaticCondensation = true;
    bool useConformingTraces = true;
    bool testHMultigrid = true;
    int levels = 2;
    bool uniform = false;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SlowStandardSolveTwoLevelNonconforming )
  {
    bool simple = false;
    bool useStaticCondensation = false;
    bool useConformingTraces = false;
    bool testHMultigrid = true;
    int levels = 2;
    bool uniform = true;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SlowStandardSolveTwoLevelHangingNodesNonconforming )
  {
    bool simple = false;
    bool useStaticCondensation = false;
    bool useConformingTraces = false;
    bool testHMultigrid = true;
    int levels = 2;
    bool uniform = false;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SlowStandardSolveTwoLevelConforming )
  {
    bool simple = false;
    bool useStaticCondensation = false;
    bool useConformingTraces = true;
    bool testHMultigrid = true;
    int levels = 2;
    bool uniform = true;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
  
  TEUCHOS_UNIT_TEST( GMGOperator, ProlongationOperatorQuad_SlowStandardSolveTwoLevelHangingNodesConforming )
  {
    bool simple = false;
    bool useStaticCondensation = false;
    bool useConformingTraces = true;
    bool testHMultigrid = true;
    int levels = 2;
    bool uniform = false;
    testProlongationOperatorQuad(simple, useStaticCondensation, useConformingTraces, testHMultigrid,
                                 levels, uniform, out, success);
  }
   */
} // namespace
