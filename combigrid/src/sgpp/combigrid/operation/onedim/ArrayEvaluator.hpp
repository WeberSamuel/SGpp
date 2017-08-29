// Copyright (C) 2008-today The SG++ project
// This file is part of the SG++ project. For conditions of distribution and
// use, please see the copyright notice provided with SG++ or at
// sgpp.sparsegrids.org

#ifndef COMBIGRID_SRC_SGPP_COMBIGRID_OPERATION_ONEDIM_ARRAYEVALUATOR_HPP_
#define COMBIGRID_SRC_SGPP_COMBIGRID_OPERATION_ONEDIM_ARRAYEVALUATOR_HPP_

#include <sgpp/combigrid/algebraic/FloatArrayVector.hpp>
#include <sgpp/combigrid/algebraic/FloatScalarVector.hpp>
#include <sgpp/combigrid/definitions.hpp>
#include <sgpp/combigrid/operation/onedim/AbstractLinearEvaluator.hpp>

#include <vector>

namespace sgpp {
namespace combigrid {

/**
 * This class takes a 1D linear evaluator operating on scalars (which is given through the template
 * type ScalarEvaluator) and uses multiple instances of it for doing multi-evaluation.
 * It is optimized to perform operations common to all of the evaluators only once.
 */
template <typename ScalarEvaluator>
class ArrayEvaluator : public AbstractLinearEvaluator<FloatArrayVector> {
  ScalarEvaluator evaluator;
  FloatArrayVector params;
  std::vector<FloatArrayVector> basisValues;
  std::vector<double> basisCoefficients;
  bool valuesComputed = false;
  std::vector<double> xValues;
  bool doesNeedParameter;

  void computeBasisValues() {
    basisValues = std::vector<FloatArrayVector>(xValues.size(), FloatArrayVector::zero());

    if (params.size() == 0) {
      auto coeff = evaluator.getBasisValues();
      for (size_t j = 0; j < coeff.size(); ++j) {
        basisValues[j].at(0) = coeff[j];
      }
    } else {
      for (size_t i = 0; i < params.size(); ++i) {
        evaluator.setParameter(params[i]);
        auto coeff = evaluator.getBasisValues();
        for (size_t j = 0; j < coeff.size(); ++j) {
          basisValues[j].at(i) = coeff[j];
        }
      }
    }

    valuesComputed = true;
  }

 public:
  explicit ArrayEvaluator(bool doesNeedParameter,
                          ScalarEvaluator evaluatorPrototype = ScalarEvaluator())
      : evaluator(evaluatorPrototype),
        params(),
        basisValues(),
        xValues(),
        doesNeedParameter(doesNeedParameter) {}

  explicit ArrayEvaluator(bool doesNeedParameter, ScalarEvaluator *evaluatorPrototype)
      : evaluator(*evaluatorPrototype),
        params(),
        basisValues(),
        xValues(),
        doesNeedParameter(doesNeedParameter) {}

  ArrayEvaluator(ArrayEvaluator<ScalarEvaluator> const &other)
      : evaluator(other.evaluator),
        params(other.params),
        basisValues(other.basisValues),
        valuesComputed(other.valuesComputed),
        xValues(other.xValues),
        doesNeedParameter(other.doesNeedParameter) {}

  ~ArrayEvaluator() {}

  std::vector<FloatArrayVector> getBasisValues() override {
    if (!valuesComputed) {
      computeBasisValues();
    }
    return basisValues;
  }

  std::vector<double> getBasisCoefficients() override { return basisCoefficients; }

  void setGridPoints(std::vector<double> const &xValues) override {
    this->xValues = xValues;
    evaluator.setGridPoints(xValues);

    valuesComputed = false;
  }

  void setFunctionValuesAtGridPoints(std::vector<double> &functionValues) override {
    basisCoefficients = functionValues;
  }

  std::shared_ptr<AbstractLinearEvaluator<FloatArrayVector>> cloneLinear() override {
    return std::shared_ptr<AbstractLinearEvaluator<FloatArrayVector>>(
        new ArrayEvaluator<ScalarEvaluator>(*this));
  }

  bool needsOrderedPoints() override { return evaluator.needsOrderedPoints(); }

  bool needsParameter() override { return doesNeedParameter; }

  void setParameter(FloatArrayVector const &param) override {
    this->params = param;
    valuesComputed = false;
  }
};

} /* namespace combigrid */
} /* namespace sgpp*/

#endif /* COMBIGRID_SRC_SGPP_COMBIGRID_OPERATION_ONEDIM_ARRAYEVALUATOR_HPP_ */