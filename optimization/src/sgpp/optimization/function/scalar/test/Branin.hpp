// Copyright (C) 2008-today The SG++ project
// This file is part of the SG++ project. For conditions of distribution and
// use, please see the copyright notice provided with SG++ or at
// sgpp.sparsegrids.org

#ifndef SGPP_OPTIMIZATION_FUNCTION_TEST_BRANIN_HPP
#define SGPP_OPTIMIZATION_FUNCTION_TEST_BRANIN_HPP

#include <sgpp/globaldef.hpp>

#include <sgpp/optimization/function/scalar/test/TestFunction.hpp>

#include <cmath>

namespace SGPP {
  namespace optimization {
    namespace test_functions {

      /**
       * Branin test function.
       *
       * Definition:
       * \f$f(\vec{x}) :=
       * \left(x_2 - 5.1 x_1^2/(4\pi^2) + 5 x_1/\pi - 6\right)^2
       * + 10 \left(1 - 1/(8\pi)\right) \cos x_1 + 10\f$,
       * \f$\vec{x} \in [-5, 10] \times [0, 15]\f$,
       * \f$\vec{x}_{\text{opt}} \in
       * \{(-\pi, 12,275)^{\mathrm{T}}, (\pi, 2,275)^{\mathrm{T}},
       * (9,42478, 2,475)^{\mathrm{T}}\}\f$,
       * \f$f_{\text{opt}} = 0.397887\f$
       * (domain scaled to \f$[0, 1]^2\f$)
       */
      class Branin : public TestFunction {
        public:
          /**
           * Constructor.
           */
          Branin();

          /**
           * Destructor.
           */
          virtual ~Branin() override;

          /**
           * Evaluates the test function.
           *
           * @param x     point \f$\vec{x} \in [0, 1]^2\f$
           * @return      \f$f(\vec{x})\f$
           */
          virtual float_t evalUndisplaced(const base::DataVector& x) override;

          /**
           * Returns minimal point and function value of the test function.
           *
           * @param[out] x minimal point
           *               \f$\vec{x}_{\text{opt}} \in [0, 1]^2\f$
           * @return       minimal function value
           *               \f$f_{\text{opt}} = f(\vec{x}_{\text{opt}})\f$
           */
          virtual float_t getOptimalPointUndisplaced(base::DataVector& x) override;

          /**
           * @param[out] clone pointer to cloned object
           */
          virtual void clone(std::unique_ptr<ScalarFunction>& clone) const override;
      };

    }
  }
}

#endif /* SGPP_OPTIMIZATION_FUNCTION_TEST_BRANIN_HPP */
