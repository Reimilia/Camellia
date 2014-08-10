//
//  Solver.h
//  Camellia
//
//  Created by Nathan Roberts on 3/3/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef Camellia_Solver_h
#define Camellia_Solver_h

#include "Epetra_LinearProblem.h"
#include "Amesos_Klu.h"
#include "AztecOO.h"

#include "CamelliaConfig.h"

// abstract class for solving Epetra_LinearProblem problems
class Solver {
protected:
  Teuchos::RCP< Epetra_LinearProblem > _problem;
public:
  virtual ~Solver() {}
  virtual Epetra_LinearProblem & problem() { return *(_problem.get()); }
  virtual void setProblem(Teuchos::RCP< Epetra_LinearProblem > problem) {
    _problem = problem;
  }
  virtual int solve() = 0; // solve with an error code response
};

// some concrete implementations follow…
class KluSolver : public Solver {
public:
  int solve() {
    Amesos_Klu klu(problem());
    return klu.Solve();
  }
};

using namespace std;

// only use MUMPS when we have MPI
#ifdef USE_MUMPS
#ifdef HAVE_MPI
#include "Amesos_Mumps.h"
class MumpsSolver : public Solver {
  int _maxMemoryPerCoreMB;
public:
  MumpsSolver(int maxMemoryPerCoreMB = 512) {
    // maximum amount of memory MUMPS may allocate per core.
    _maxMemoryPerCoreMB = maxMemoryPerCoreMB;
  }
  
  int solve() {
    Amesos_Mumps mumps(problem());
    int numProcs=1;
    int rank=0;
    
    int previousSize = 0;
#ifdef HAVE_MPI
    rank     = Teuchos::GlobalMPISession::getRank();
    numProcs = Teuchos::GlobalMPISession::getNProc();
    mumps.SetICNTL(28, 2); // 2: parallel analysis
    mumps.SetICNTL(29, 1); // 1: use PT-SCOTCH
    
//    int minSize = max(infog[26-1], infog[16-1]);
//    // want to set ICNTL 23 to a size "significantly larger" than minSize
//    int sizeToSet = max(2 * minSize, previousSize*2);
//    sizeToSet = min(sizeToSet, _maxMemoryPerCoreMB);
//    previousSize = sizeToSet;
    //    mumps.SetICNTL(23, sizeToSet);
    
    // not sure why we shouldn't just do this: (I don't think MUMPS will allocate as much as we allow it, unless it thinks it needs it)
    mumps.SetICNTL(1,6); // set output stream for errors (this is supposed to be the default, but maybe Amesos clobbers it?)
//    int sizeToSet = _maxMemoryPerCoreMB;
//    cout << "setting ICNTL 23 to " << sizeToSet << endl;
//    mumps.SetICNTL(23, sizeToSet);
#else
#endif

    mumps.SymbolicFactorization();
    mumps.NumericFactorization();
    int relaxationParam = 0; // the default
    int* info = mumps.GetINFO();
    int* infog = mumps.GetINFOG();
    
    int numErrors = 0;
    while (info[0] < 0) { // error occurred
      info = mumps.GetINFO(); // not sure if these can change locations between invocations -- just in case...
      infog = mumps.GetINFOG();
      
      numErrors++;
      if (rank == 0) {
        if (infog[0] == -9) {
          int minSize = infog[26-1];
          // want to set ICNTL 23 to a size "significantly larger" than minSize
          int sizeToSet = max(2 * minSize, previousSize*2);
          sizeToSet = min(sizeToSet, _maxMemoryPerCoreMB);
          mumps.SetICNTL(23, sizeToSet);
          cout << "\nMUMPS memory allocation too small.  Setting to: " << sizeToSet << " MB/core." << endl;
          previousSize = sizeToSet;
        } else if (infog[0] == -7) {
          // some error related to an integer array allocation.
          // since I'm not sure how to determine how much we previously had, we'll just try again with the max
          int sizeToSet = _maxMemoryPerCoreMB;
          cout << "\nMUMPS encountered an error allocating an integer workspace of size " << infog[2-1] << " MB.\n";
          cout << "-- perhaps it's running into our allocation limit?? Setting the allocation limit to ";
          cout << sizeToSet << " MB/core." << endl;
          mumps.SetICNTL(23, sizeToSet);
        } else if (infog[0]==-13) { // error during a Fortran ALLOCATE statement
          if (previousSize > 0) {
            int sizeToSet = 3 * previousSize / 4; // reduce size by 25%
            mumps.SetICNTL(23, sizeToSet);
            cout << "MUMPS memory allocation error -13; likely indicates we're out of memory.  Reducing by 25%; setting to: " << sizeToSet << " MB/core." << endl;
          } else {
            cout << "MUMPS memory allocation error -13, but previousSize was 0.  (Unhandled case)." << endl;
          }
        } else {
          cout << "MUMPS encountered unhandled error code " << infog[0] << endl;
          for (int i=0; i<40; i++) {
            cout << "infog[" << setw(2) << i+1 << "] = " << infog[i] << endl; // i+1 because 1-based indices are used in MUMPS manual
          }
          TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "Unhandled MUMPS error code");
        }
      }
      mumps.SymbolicFactorization();
      mumps.NumericFactorization();
      if (numErrors > 20) {
        if (rank==0) cout << "Too many errors during MUMPS factorization.  Quitting.\n";
        break;
      }
    }
    return mumps.Solve();
  }
};
#else
class MumpsSolver : public Solver {
public:
  int solve() {
    cout << "ERROR: no MUMPS support for non-MPI runs yet (because Nate hasn't built MUMPS for his serial-debug Trilinos).\n";
    return -1;
  }
};
#endif

#endif

#endif
