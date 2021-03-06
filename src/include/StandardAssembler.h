// NOTE: This is deprecated by the conversion to Tpetra
#ifndef STANDARD_ASSEMBLER
#define STANDARD_ASSEMBLER

// @HEADER
//
// Copyright © 2011 Sandia Corporation. All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
// 1. Redistributions of source code must retain the above copyright notice, this list of
// conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice, this list of
// conditions and the following disclaimer in the documentation and/or other materials
// provided with the distribution.
// 3. The name of the author may not be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
// OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
// BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Nate Roberts (nate@nateroberts.com).
//
// @HEADER

#include "Assembler.h"
#include "Epetra_FECrsMatrix.h"
#include "Epetra_FEVector.h"
#include "BF.h" // to compute stiffness

#include "Element.h"

namespace Camellia
{
class StandardAssembler : public Assembler
{
  TSolutionPtr<double> _solution;
public:
  StandardAssembler(TSolutionPtr<double> solution)
  {
    _solution = solution;
  };
  Epetra_Map getPartMap();
  Epetra_FECrsMatrix initializeMatrix();
  Epetra_FEVector initializeVector();

  //  Teuchos::RCP<Epetra_LinearProblem> assembleProblem();
  //  Epetra_FECrsMatrix assembleProblem();
  void assembleProblem(Epetra_FECrsMatrix &globalStiffMatrix, Epetra_FEVector &rhsVector);

  void distributeDofs(Epetra_FEVector dofs);

  Intrepid::FieldContainer<double> getRHS(ElementPtr elem);
  Intrepid::FieldContainer<double> getOverdeterminedStiffness(ElementPtr elem);
  Intrepid::FieldContainer<double> getIPMatrix(ElementPtr elem);
  int numTestDofsForElem(ElementPtr elem);
  int numTrialDofsForElem(ElementPtr elem);
  void applyBCs(Epetra_FECrsMatrix &globalStiffMatrix, Epetra_FEVector &rhsVector);
  Intrepid::FieldContainer<double> getSubVector(Epetra_FEVector u, ElementPtr elem);
  void getElemData(ElementPtr elem, Intrepid::FieldContainer<double> &K, Intrepid::FieldContainer<double> &f);
  TSolutionPtr<double> solution()
  {
    return _solution;
  }
};
}

#endif
