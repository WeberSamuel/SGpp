// Copyright (C) 2008-today The SG++ project
// This file is part of the SG++ project. For conditions of distribution and
// use, please see the copyright notice provided with SG++ or at
// sgpp.sparsegrids.org

#ifndef OPERATIONNAIVEEVALMODBSPLINECLENSHAWCURTIS_HPP
#define OPERATIONNAIVEEVALMODBSPLINECLENSHAWCURTIS_HPP

#include <sgpp/globaldef.hpp>
#include <sgpp/base/operation/hash/OperationNaiveEval.hpp>
#include <sgpp/base/grid/GridStorage.hpp>
#include <sgpp/base/operation/hash/common/basis/BsplineModifiedClenshawCurtisBasis.hpp>
#include <sgpp/base/datatypes/DataVector.hpp>

namespace SGPP {
  namespace base {

    /**
     * Operation for evaluating modified Clenshaw-Curtis B-spline
     * linear combinations on Noboundary grids.
     */
    class OperationNaiveEvalModBsplineClenshawCurtis :
      public OperationNaiveEval {
      public:
        /**
         * Constructor.
         *
         * @param storage   storage of the sparse grid
         * @param degree    B-spline degree
         */
        OperationNaiveEvalModBsplineClenshawCurtis(
          GridStorage* storage, size_t degree) :
          storage(storage), base(degree) {
        }

        /**
         * Virtual destructor.
         */
        virtual ~OperationNaiveEvalModBsplineClenshawCurtis() {
        }

        /**
         * @param alpha     coefficient vector
         * @param point     evaluation point
         * @return          value of linear combination
         */
        virtual float_t eval(DataVector& alpha, DataVector& point);

      protected:
        /// storage of the sparse grid
        GridStorage* storage;
        /// 1D B-spline basis
        SBsplineModifiedClenshawCurtisBase base;
    };

  }
}

#endif /* OPERATIONNAIVEEVALMODBSPLINECLENSHAWCURTIS_HPP */