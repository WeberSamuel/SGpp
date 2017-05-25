// Copyright (C) 2008-today The SG++ project
// This file is part of the SG++ project. For conditions of distribution and
// use, please see the copyright notice provided with SG++ or at
// sgpp.sparsegrids.org

#include <sgpp/base/operation/hash/OperationSecondMomentBsplineBoundary.hpp>
#include <sgpp/base/grid/type/BsplineBoundaryGrid.hpp>
#include <sgpp/base/exception/application_exception.hpp>
#include <sgpp/base/tools/GaussLegendreQuadRule1D.hpp>

#include <sgpp/globaldef.hpp>
#include <algorithm>

namespace sgpp {
namespace base {

double OperationSecondMomentBsplineBoundary::doQuadrature(DataVector& alpha,
                                                          DataMatrix* bounds) {
  // handle bounds
  GridStorage& storage = grid->getStorage();
  size_t numDims = storage.getDimension();

  // check if the boundaries are given in the right shape
  if (bounds != nullptr && (bounds->getNcols() != 2 || bounds->getNrows() != numDims)) {
    throw application_exception(
        "OperationSecondMomentBsplineBoundary::doQuadrature - bounds matrix has the wrong shape");
  }
  size_t p = dynamic_cast<sgpp::base::BsplineBoundaryGrid*>(grid)->getDegree();
  double pDbl = static_cast<double>(p);
  double res = 0;
  double tmpres = 1;
  base::index_t index;
  double indexDbl;
  base::level_t level;
  double xlower = 0.0;
  double xupper = 0.0;
  const size_t pp1h = (p + 1) >> 1;  // (p + 1) / 2
  const double pp1hDbl = static_cast<double>(pp1h);
  const size_t quadOrder = static_cast<size_t>(ceil(pDbl / 2.)) + 2;
  base::SBasis& basis = const_cast<base::SBasis&>(grid->getBasis());
  base::DataVector coordinates;
  base::DataVector weights;
  base::GaussLegendreQuadRule1D gauss;
  gauss.getLevelPointsAndWeightsNormalized(quadOrder, coordinates, weights);

  for (GridStorage::grid_map_iterator iter = storage.begin(); iter != storage.end(); iter++) {
    tmpres = 1.;

    for (size_t dim = 0; dim < storage.getDimension(); dim++) {
      index = iter->first->getIndex(dim);
      indexDbl = static_cast<double>(index);
      level = iter->first->getLevel(dim);
      double hInv = static_cast<double>(1 << level);
      xlower = bounds == nullptr ? 0.0 : bounds->get(dim, 0);
      xupper = bounds == nullptr ? 1.0 : bounds->get(dim, 1);
      double width = xupper - xlower;

      size_t start = ((index > pp1h) ? 0 : (pp1h - index));
      size_t stop = static_cast<size_t>(std::min(pDbl, hInv + pp1hDbl - indexDbl - 1));
      double scaling = 1./hInv;  // for a single BsplineBoundary section
      double sum_1d = 0.;
      for (size_t n = start; n <= stop; n++) {
        double offset = scaling * static_cast<double>(n + index - pp1h);
        for (size_t c = 0; c < quadOrder; c++) {
          const double x = offset + scaling * coordinates[c];
          sum_1d += weights[c] * x * x * basis.eval(level, index, x);
        }
      }
      sum_1d *= scaling;
      tmpres *=
        width * sum_1d + xlower * basis.getIntegral(level, index);
    }

    res += alpha.get(iter->second) * tmpres;
  }

  return res;
}

}  // namespace base
}  // namespace sgpp
