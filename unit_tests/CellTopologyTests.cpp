//
//  CellTopologyTests.cpp
//  Camellia
//
//  Created by Nate Roberts on 12/11/14.
//
//

#include "Teuchos_UnitTestHarness.hpp"
#include "Teuchos_UnitTestHelpers.hpp"

#include "Intrepid_FieldContainer.hpp"

#include "CellTopology.h"

#include "CamelliaDebugUtility.h"

using namespace Intrepid;
using namespace Camellia;

namespace {
  // helper methods for some tests:
  bool checkDimension(CellTopoPtr cellTopo) {
    return cellTopo->getDimension() == cellTopo->getShardsTopology().getDimension() + cellTopo->getTensorialDegree();
  }
  
  bool checkPermutationCount(CellTopoPtr cellTopo) {
    unsigned actualPermutationCount = cellTopo->getNodePermutationCount();
    unsigned expectedPermutationCount;
    if (cellTopo->isHypercube()) {
      expectedPermutationCount = 1;
      for (int d=0; d<cellTopo->getDimension(); d++) {
        expectedPermutationCount *= 2 * (d+1);
      }
    } else {
      int shardsPermutationCount = cellTopo->getShardsTopology().getNodePermutationCount();
      expectedPermutationCount = shardsPermutationCount;
      if (cellTopo->getTensorialDegree() == 1)  expectedPermutationCount *= 2;
      else if (cellTopo->getTensorialDegree() > 1) {
        cout << "Test failure: unsupported tensorial degree.\n";
        return false;
      }
    }
    if (expectedPermutationCount != actualPermutationCount) {
      cout << "FAILURE: expected permutation count " << expectedPermutationCount << " did not match actual " << actualPermutationCount << endl;
    } else {
      //    cout << "actual permutation count matches expected: " << actualPermutationCount << endl;
    }
    return (expectedPermutationCount == actualPermutationCount);
  }
  
  bool checkPermutations(CellTopoPtr cellTopo) {
    bool success = true;
    map< vector<unsigned>, unsigned > knownPermutations; // from permutation to its ordinal
    // true if all permutations are distinct and inverses do in fact invert
    int permutationCount = cellTopo->getNodePermutationCount();
    int nodeCount = cellTopo->getNodeCount();
    for (unsigned permutationNumber = 0; permutationNumber < permutationCount; permutationNumber++) {
      vector<unsigned> permutedNodes(nodeCount);
      set<unsigned> permutedNodeSet;
      for (unsigned node=0; node<nodeCount; node++) {
        unsigned permutedNode = cellTopo->getNodePermutation(permutationNumber, node);
        permutedNodes[node] = permutedNode;
        unsigned inversePermutedNode = cellTopo->getNodePermutationInverse(permutationNumber, permutedNode);
        permutedNodeSet.insert(permutedNode);
        if (inversePermutedNode != node) {
          success = false;
          cout << "Test failure: permutation number " << permutationNumber << " maps " << node << " to " << permutedNode;
          cout << ", but its inverse maps " << permutedNode << " to " << inversePermutedNode << endl;
        }
      }
      if (permutedNodeSet.size() < permutedNodes.size()) {
        cout << "Test failure: permutation number " << permutationNumber << " has duplicate entries.\n";
        Camellia::print("permutedNodes", permutedNodes );
      }
      if (knownPermutations.find(permutedNodes) != knownPermutations.end()) {
        cout << "Test failure: permutation number " << permutationNumber << " matched a node ordering previously ";
        cout << "generated by permutation number " << knownPermutations[permutedNodes] << endl;
        vector< vector<unsigned> > permList(permutationCount);
        for (map< vector<unsigned>, unsigned >::iterator knownPermIt = knownPermutations.begin(); knownPermIt != knownPermutations.end(); knownPermIt++) {
          permList[knownPermIt->second] = knownPermIt->first;
        }
        for (int i=0; i<permList.size(); i++) {
          ostringstream permNum;
          permNum << "permutation " << i;
          Camellia::print(permNum.str(), permList[i]);
        }
        
        success = false;
      }
      knownPermutations[permutedNodes] = permutationNumber;
    }
    
    return success;
  }
  
  vector< CellTopoPtr > getShardsTopologies() {
    vector< CellTopoPtr > shardsTopologies;
    
    shardsTopologies.push_back(CellTopology::point());
    shardsTopologies.push_back(CellTopology::line());
    shardsTopologies.push_back(CellTopology::quad());
    shardsTopologies.push_back(CellTopology::triangle());
    shardsTopologies.push_back(CellTopology::hexahedron());
    //  shardsTopologies.push_back(CellTopology::tetrahedron()); // tetrahedron not yet supported by permutation
    return shardsTopologies;
  }
  
  TEUCHOS_UNIT_TEST( CellTopology, InitializeNodesLine)
  {
    FieldContainer<double> lineNodes_x(2,1);
    
    lineNodes_x(0,0) =  0;
    lineNodes_x(1,0) =  1;
    
    vector< FieldContainer<double> > tensorComponentNodes(1);
    tensorComponentNodes[0] = lineNodes_x;
    
    FieldContainer<double> expectedNodes(2,1);
    // expect to get all the x0's then all the x1's, etc.
    expectedNodes(0,0) = lineNodes_x(0,0);
    expectedNodes(1,0) = lineNodes_x(1,0);
    
    FieldContainer<double> actualNodes(2,1);
    
    shards::CellTopology shardsLine(shards::getCellTopologyData<shards::Line<2> >() );
    
    int tensorialDegree = 0; // line + 0 for 1D
    CellTopoPtr lineTopo = CellTopology::cellTopology(shardsLine, tensorialDegree);
    
    lineTopo->initializeNodes(tensorComponentNodes, actualNodes);
    
    TEST_COMPARE_FLOATING_ARRAYS(expectedNodes, actualNodes, 1e-15);
  }

  TEUCHOS_UNIT_TEST( CellTopology, InitializeNodesQuad)
  {
    FieldContainer<double> lineNodes_x(2,1);
    FieldContainer<double> lineNodes_y(2,1);
    
    lineNodes_x(0,0) =  0;
    lineNodes_x(1,0) =  1;
    lineNodes_y(0,0) =  2;
    lineNodes_y(1,0) =  4;
    
    vector< FieldContainer<double> > tensorComponentNodes(2);
    tensorComponentNodes[0] = lineNodes_x;
    tensorComponentNodes[1] = lineNodes_y;
    
    FieldContainer<double> expectedNodes(4,2);
    // expect to see a copy of lineNodes_x with y0 as last dimension, then another copy with y1
    expectedNodes(0,0) = lineNodes_x(0,0);
    expectedNodes(0,1) = lineNodes_y(0,0);
    
    expectedNodes(1,0) = lineNodes_x(1,0);
    expectedNodes(1,1) = lineNodes_y(0,0);
    
    expectedNodes(2,0) = lineNodes_x(0,0);
    expectedNodes(2,1) = lineNodes_y(1,0);
    
    expectedNodes(3,0) = lineNodes_x(1,0);
    expectedNodes(3,1) = lineNodes_y(1,0);
    
    FieldContainer<double> actualNodes(4,2);
    actualNodes.initialize(-17); // to make clear what, if anything, is not being set
    
    shards::CellTopology shardsLine(shards::getCellTopologyData<shards::Line<2> >() );
    
    int tensorialDegree = 1; // line + 1 for 2D
    CellTopoPtr quadTopo = CellTopology::cellTopology(shardsLine, tensorialDegree);
    
    quadTopo->initializeNodes(tensorComponentNodes, actualNodes);
    
    TEST_COMPARE_FLOATING_ARRAYS(expectedNodes, actualNodes, 1e-15);
  }
  
  TEUCHOS_UNIT_TEST( CellTopology, InitializeNodesHexahedron)
  {
    FieldContainer<double> lineNodes_x(2,1);
    FieldContainer<double> lineNodes_y(2,1);
    FieldContainer<double> lineNodes_z(2,1);
    
    lineNodes_x(0,0) =  0;
    lineNodes_x(1,0) =  1;
    lineNodes_y(0,0) =  2;
    lineNodes_y(1,0) =  4;
    lineNodes_z(0,0) = -3;
    lineNodes_z(1,0) =  3;

    vector< FieldContainer<double> > tensorComponentNodes(3);
    tensorComponentNodes[0] = lineNodes_x;
    tensorComponentNodes[1] = lineNodes_y;
    tensorComponentNodes[2] = lineNodes_z;
    
    FieldContainer<double> expectedNodes(8,3);
    expectedNodes(0,0) = lineNodes_x(0,0);
    expectedNodes(0,1) = lineNodes_y(0,0);
    expectedNodes(0,2) = lineNodes_z(0,0);
    
    expectedNodes(1,0) = lineNodes_x(1,0);
    expectedNodes(1,1) = lineNodes_y(0,0);
    expectedNodes(1,2) = lineNodes_z(0,0);
    
    expectedNodes(2,0) = lineNodes_x(0,0);
    expectedNodes(2,1) = lineNodes_y(1,0);
    expectedNodes(2,2) = lineNodes_z(0,0);
    
    expectedNodes(3,0) = lineNodes_x(1,0);
    expectedNodes(3,1) = lineNodes_y(1,0);
    expectedNodes(3,2) = lineNodes_z(0,0);

    
    expectedNodes(4,0) = lineNodes_x(0,0);
    expectedNodes(4,1) = lineNodes_y(0,0);
    expectedNodes(4,2) = lineNodes_z(1,0);
    
    expectedNodes(5,0) = lineNodes_x(1,0);
    expectedNodes(5,1) = lineNodes_y(0,0);
    expectedNodes(5,2) = lineNodes_z(1,0);
    
    expectedNodes(6,0) = lineNodes_x(0,0);
    expectedNodes(6,1) = lineNodes_y(1,0);
    expectedNodes(6,2) = lineNodes_z(1,0);
    
    expectedNodes(7,0) = lineNodes_x(1,0);
    expectedNodes(7,1) = lineNodes_y(1,0);
    expectedNodes(7,2) = lineNodes_z(1,0);

    FieldContainer<double> actualNodes(8,3);
    actualNodes.initialize(-17); // to make clear what, if anything, is not being set
    
    shards::CellTopology shardsLine(shards::getCellTopologyData<shards::Line<2> >() );

    int tensorialDegree = 2; // line + 2 for 3D
    CellTopoPtr hexTopo = CellTopology::cellTopology(shardsLine, tensorialDegree);
    
    hexTopo->initializeNodes(tensorComponentNodes, actualNodes);
    
    TEST_COMPARE_FLOATING_ARRAYS(expectedNodes, actualNodes, 1e-15);
  }
  
  TEUCHOS_UNIT_TEST( CellTopology, ShardsTopologiesPermutations )
  {
    vector<CellTopoPtr> shardsTopologies = getShardsTopologies();
    for (int topoOrdinal=0; topoOrdinal < shardsTopologies.size(); topoOrdinal++) {
      CellTopoPtr cellTopo = shardsTopologies[topoOrdinal];
      TEST_ASSERT(checkPermutationCount(cellTopo));
//      if (! checkPermutationCount(cellTopo)) {
//        cout << "testShardsTopologiesPermutations failed checkPermutationCount() for " << cellTopo->getShardsTopology().getName() << endl;
//        success = false;
//      }
      TEST_ASSERT(checkPermutations(cellTopo));
//      if (! checkPermutations(cellTopo)) {
//        cout << "testShardsTopologiesPermutations failed for " << cellTopo->getShardsTopology().getName() << endl;
//        success = false;
//      }
    }
  }
  
  TEUCHOS_UNIT_TEST( CellTopology, OneTensorTopologiesPermutations )
  {
    // one tensorial dimension
    int tensorialDegree = 1;
    vector<CellTopoPtr> shardsTopologies = getShardsTopologies();
    for (int topoOrdinal=0; topoOrdinal < shardsTopologies.size(); topoOrdinal++) {
      shards::CellTopology shardsTopo = shardsTopologies[topoOrdinal]->getShardsTopology();
      CellTopoPtr cellTopo = CellTopology::cellTopology(shardsTopo, tensorialDegree);
      TEST_ASSERT(checkDimension(cellTopo));
//      if (! checkDimension(cellTopo)) {
//        cout << "testOneTensorTopologiesPermutations failed checkDimension() for " << cellTopo->getShardsTopology().getName() << endl;
//        success = false;
//      }
      TEST_ASSERT(checkPermutationCount(cellTopo));
//      if (! checkPermutationCount(cellTopo)) {
//        cout << "testOneTensorTopologiesPermutations failed checkPermutationCount() for " << cellTopo->getShardsTopology().getName() << endl;
//        success = false;
//      }
      TEST_ASSERT(checkPermutations(cellTopo));
//      if (! checkPermutations(cellTopo)) {
//        cout << "testOneTensorTopologiesPermutations failed for " << cellTopo->getShardsTopology().getName() << endl;
//        success = false;
//      }
    }
  }
  
  TEUCHOS_UNIT_TEST( CellTopology, MultiTensorTopologiesPermutations )
  {
    int minDimension = 1;
    int maxDimension = 4; // tested up to 6, but 4 is all we really are interested in...
    
    vector< CellTopoPtr > shardsHyperCubeTopos;
    shardsHyperCubeTopos.push_back(CellTopology::point());
    shardsHyperCubeTopos.push_back(CellTopology::line());
    shardsHyperCubeTopos.push_back(CellTopology::quad());
    shardsHyperCubeTopos.push_back(CellTopology::hexahedron());
    
    for (int shardsOrdinal=0; shardsOrdinal<shardsHyperCubeTopos.size(); shardsOrdinal++) {
      shards::CellTopology shardsTopo = shardsHyperCubeTopos[shardsOrdinal]->getShardsTopology();
      
      int minTensorialDegree = (minDimension > shardsTopo.getDimension()) ? minDimension - shardsTopo.getDimension() : 0;
      int maxTensorialDegree = (minDimension > shardsTopo.getDimension()) ? maxDimension - shardsTopo.getDimension() : 0;
      
      for (int tensorialDegree=minTensorialDegree; tensorialDegree <= maxTensorialDegree; tensorialDegree++) {
        CellTopoPtr cellTopo = CellTopology::cellTopology(shardsTopo, tensorialDegree);
        
        TEST_ASSERT(checkDimension(cellTopo));
//        if (! checkDimension(cellTopo)) {
//          cout << "testMultiTensorTopologiesPermutations failed checkDimension() for " << cellTopo->getShardsTopology().getName() << endl;
//          success = false;
//        }
        TEST_ASSERT(checkPermutationCount(cellTopo));
//        if (! checkPermutationCount(cellTopo)) {
//          cout << "testMultiTensorTopologiesPermutations failed checkPermutationCount() for " << cellTopo->getShardsTopology().getName() << endl;
//          success = false;
//        }
        TEST_ASSERT(checkPermutations(cellTopo));
//        if (! checkPermutations(cellTopo)) {
//          cout << "testMultiTensorTopologiesPermutations failed for " << cellTopo->getShardsTopology().getName();
//          cout << " with tensorial degree " << tensorialDegree << endl;
//          success = false;
//        }
      }
    }
  }
} // namespace