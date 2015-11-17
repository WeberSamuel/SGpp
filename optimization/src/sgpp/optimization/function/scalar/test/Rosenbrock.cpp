// Copyright (C) 2008-today The SG++ project
// This file is part of the SG++ project. For conditions of distribution and
// use, please see the copyright notice provided with SG++ or at
// sgpp.sparsegrids.org

#include <sgpp/globaldef.hpp>
#include <sgpp/optimization/function/scalar/test/Rosenbrock.hpp>

#include <cmath>

namespace SGPP {
  namespace optimization {
    namespace test_functions {

      Rosenbrock::Rosenbrock(size_t d) : TestFunction(d) {
      }

      Rosenbrock::~Rosenbrock() {
      }

      float_t Rosenbrock::evalUndisplaced(const base::DataVector& x) {
        float_t result = 0.0;

        float_t xt = 15.0 * x[0] - 5.0;

        for (size_t t = 1; t < d; t++) {
          const float_t xtm1 = xt;
          xt = 15.0 * x[t] - 5.0;

          const float_t tmp1 = xt - xtm1 * xtm1;
          const float_t tmp2 = 1.0 - xtm1;
          result += 100.0 * tmp1 * tmp1 + tmp2 * tmp2;
        }

        return result;
      }

      float_t Rosenbrock::getOptimalPointUndisplaced(base::DataVector& x) {
        x.resize(d);
        x.setAll(0.4);
        return 0.0;
      }

      void Rosenbrock::clone(std::unique_ptr<ScalarFunction>& clone) const {
        clone = std::unique_ptr<ScalarFunction>(new Rosenbrock(*this));
      }

    }
  }
}
