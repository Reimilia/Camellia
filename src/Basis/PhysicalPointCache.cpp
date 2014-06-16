//
//  PhysicalPointCache.cpp
//  Camellia-debug
//
//  Created by Nate Roberts on 6/6/14.
//
//

#include "PhysicalPointCache.h"

PhysicalPointCache::PhysicalPointCache(const FieldContainer<double> &physCubPoints) : BasisCache() {
  _physCubPoints = physCubPoints;
}
const FieldContainer<double> & PhysicalPointCache::getPhysicalCubaturePoints() { // overrides super
  return _physCubPoints;
}
FieldContainer<double> & PhysicalPointCache::writablePhysicalCubaturePoints() { // allows overwriting the contents
  return _physCubPoints;
}