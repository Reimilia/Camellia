#ifndef DPG_CONFECTION_PROBLEM
#define DPG_CONFECTION_PROBLEM

#include "BC.h"
#include "RHS.h"

#include "ConfusionBilinearForm.h"

class ConfectionProblem : public RHS, public BC {
 private:
  Teuchos::RCP<ConfusionBilinearForm> _cbf;
public:
  ConfectionProblem( Teuchos::RCP<ConfusionBilinearForm> cbf) : RHS(), BC() {
    _cbf = cbf;
  }
    
  // RHS:
  bool nonZeroRHS(int testVarID) {
    return false;
    //    return testVarID == ConfusionBilinearForm::V;
  }
  
  void rhs(int testVarID, FieldContainer<double> &physicalPoints, FieldContainer<double> &values) {
    int numCells = physicalPoints.dimension(0);
    int numPoints = physicalPoints.dimension(1);
    int spaceDim = physicalPoints.dimension(2);
    values.resize(numCells,numPoints);
    //    values.initialize(1.0);
    values.initialize(0.0);
  }
  
  // BC
  bool bcsImposed(int varID) {
    //    return varID == ConfusionBilinearForm::U_HAT;
    return varID == ConfusionBilinearForm::BETA_N_U_MINUS_SIGMA_HAT;
  }
  
  virtual void imposeBC(int varID, FieldContainer<double> &physicalPoints, 
                        FieldContainer<double> &unitNormals,
                        FieldContainer<double> &dirichletValues,
                        FieldContainer<bool> &imposeHere) {
    int numCells = physicalPoints.dimension(0);
    int numPoints = physicalPoints.dimension(1);
    int spaceDim = physicalPoints.dimension(2);
    double tol = 1e-14;
    double x_cut = .50;
    double y_cut = .50;
    TEST_FOR_EXCEPTION( spaceDim != 2, std::invalid_argument, "spaceDim != 2" );
    for (int cellIndex=0; cellIndex < numCells; cellIndex++) {
      for (int ptIndex=0; ptIndex < numPoints; ptIndex++) {
        double x = physicalPoints(cellIndex, ptIndex, 0);
        double y = physicalPoints(cellIndex, ptIndex, 1);
	imposeHere(cellIndex,ptIndex) = false;
	if ( (abs(x)<1e-14) || (abs(y)<1e-14) ) { // if it's on the inflow
	  imposeHere(cellIndex,ptIndex) = true;

	  double scaling=1.0;
	  if (!bcsImposed(ConfusionBilinearForm::U_HAT)){
	    double beta_n=0.0;
	    vector<double> beta = _cbf->getBeta();	    
	    for (int dimInd = 0;dimInd<spaceDim;dimInd++){
	      beta_n += beta[dimInd]*unitNormals(cellIndex,ptIndex,dimInd);
	      // assumes sigma ~ 0
	    }
	    scaling = beta_n;
	  } else {
	    scaling = 1.0; // don't weight at all if just applying ot the trace
	  }
	  if ( (abs(x) < 1e-14) && (y<y_cut) ) { // x basically 0 ==> u = 1 - y	  
	    dirichletValues(cellIndex,ptIndex) = (1.0 - y/y_cut)*scaling;
	    //          dirichletValues(cellIndex,ptIndex) = exp(-1.0/(1.0-y*y)); // bump function
	  } else if ( (abs(y) < 1e-14) &&  (x<x_cut) ) { // y basically 0 ==> u = 1 - x
	    dirichletValues(cellIndex,ptIndex) = (1.0 - x/x_cut)*scaling;
	    //          dirichletValues(cellIndex,ptIndex) = exp(-1.0/(1.0-x*x)); // bump function
	  } else {	    
	    dirichletValues(cellIndex,ptIndex) = 0.0;
	  }
	} // end of if-impose
      }
    }
  }
};
#endif
