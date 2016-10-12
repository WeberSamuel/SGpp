// Copyright (C) 2008-today The SG++ project
// This file is part of the SG++ project. For conditions of distribution and
// use, please see the copyright notice provided with SG++ or at
// sgpp.sparsegrids.org

#include <sgpp/datadriven/datamining/modules/fitting/ModelFittingLeastSquares.hpp>

#include <sgpp/datadriven/DatadrivenOpFactory.hpp>
#include <sgpp/datadriven/algorithm/SystemMatrixLeastSquaresIdentity.hpp>
#include <sgpp/solver/sle/BiCGStab.hpp>
#include <sgpp/solver/sle/ConjugateGradients.hpp>

#include <sgpp/base/exception/application_exception.hpp>
#include <sgpp/base/grid/generation/functors/SurplusRefinementFunctor.hpp>
#include <sgpp/base/operation/hash/OperationMultipleEval.hpp>

// TODO(lettrich): allow different regularization types
// TODO(lettrich): allow different refinement types
// TODO(lettrich): allow different refinement criteria

using sgpp::base::DataMatrix;
using sgpp::base::DataVector;
using sgpp::base::SurplusRefinementFunctor;

using sgpp::base::application_exception;

using sgpp::solver::SLESolver;
using sgpp::solver::ConjugateGradients;
using sgpp::solver::BiCGStab;
using sgpp::solver::SLESolverType;

namespace sgpp {
namespace datadriven {

ModelFittingLeastSquares::ModelFittingLeastSquares(const FitterConfigurationLeastSquares& config)
    : datadriven::ModelFittingBase(),
      config(config),
      systemMatrix(nullptr),
      solver(nullptr),
      implementationConfig(OperationMultipleEvalConfiguration()) {
  solver = std::shared_ptr<SLESolver>(buildSolver(this->config));
}

ModelFittingLeastSquares::~ModelFittingLeastSquares() {}

void ModelFittingLeastSquares::fit(Dataset& dataset) {
  // clear model
  alpha.reset();
  grid.reset();
  systemMatrix.reset();

  // build grid
  auto gridConfig = config.getGridConfig();
  gridConfig.dim_ = dataset.getDimension();
  config.setGridConfig(gridConfig);
  initializeGrid(config.getGridConfig());
  // build surplus vector
  alpha = std::make_shared<DataVector>(grid->getSize());

  // create sytem matrix
  systemMatrix =
      std::shared_ptr<DMSystemMatrixBase>(buildSystemMatrix(dataset.getData(), config.getLambda()));

  // create right hand side and system matrix
  auto b = std::make_unique<DataVector>(grid->getSize());
  systemMatrix->generateb(dataset.getTargets(), *b);

  configureSolver(config, *solver, FittingSolverState::solve);
  solver->solve(*systemMatrix, *alpha, *b, true, true, DEFAULT_RES_THRESHOLD);
}

void ModelFittingLeastSquares::refine() {
  if (grid != nullptr) {
    // create refinement functor
    SurplusRefinementFunctor refinementFunctor(*alpha, config.getRefinementConfig().noPoints_,
                                               config.getRefinementConfig().threshold_);
    // refine grid
    grid->getGenerator().refine(refinementFunctor);

    // tell the SLE manager that the grid changed (for interal data structures)
    // systemMatrix->prepareGrid(); -> empty statement!
    alpha->resizeZero(grid->getSize());

    throw application_exception(
        "ModelFittingLeastSquares: Can't refine before initial grid is created");
  }
}

void ModelFittingLeastSquares::update(Dataset& dataset) {
  if (grid != nullptr) {
    // create sytem matrix
    systemMatrix.reset();
    systemMatrix = std::shared_ptr<DMSystemMatrixBase>(
        buildSystemMatrix(dataset.getData(), config.getLambda()));

    auto b = std::make_unique<DataVector>(grid->getSize());
    systemMatrix->generateb(dataset.getTargets(), *b);

    configureSolver(config, *solver, FittingSolverState::refine);
    solver->solve(*systemMatrix, *alpha, *b, true, true, DEFAULT_RES_THRESHOLD);
  } else {
    fit(dataset);
  }
}

DMSystemMatrixBase* ModelFittingLeastSquares::buildSystemMatrix(DataMatrix& trainDataset,
                                                                double lambda) {
  auto systemMatrix = new SystemMatrixLeastSquaresIdentity(*grid, trainDataset, lambda);
  systemMatrix->setImplementation(implementationConfig);

  return static_cast<DMSystemMatrixBase*>(systemMatrix);
}

SLESolver* ModelFittingLeastSquares::buildSolver(FitterConfiguration& config) {
  SLESolver* solver;

  if (config.getSolverRefineConfig().type_ == SLESolverType::CG) {
    solver = new ConjugateGradients(config.getSolverFinalConfig().maxIterations_,
                                    config.getSolverFinalConfig().eps_);
  } else if (config.getSolverRefineConfig().type_ == SLESolverType::BiCGSTAB) {
    solver = new BiCGStab(config.getSolverFinalConfig().maxIterations_,
                          config.getSolverFinalConfig().eps_);
  } else {
    throw application_exception(
        "ModelFittingLeastSquares: An unsupported SLE solver type was "
        "chosen!");
  }
  return solver;
}

void ModelFittingLeastSquares::configureSolver(FitterConfiguration& config, SLESolver& solver,
                                               FittingSolverState solverState) {
  switch (solverState) {
    case FittingSolverState::solve:
      solver.setMaxIterations(config.getSolverFinalConfig().maxIterations_);
      solver.setEpsilon(config.getSolverFinalConfig().eps_);
      break;
    case FittingSolverState::refine:
      solver.setMaxIterations(config.getSolverRefineConfig().maxIterations_);
      solver.setEpsilon(config.getSolverRefineConfig().eps_);
      break;
    default:
      solver.setMaxIterations(config.getSolverFinalConfig().maxIterations_);
      solver.setEpsilon(config.getSolverFinalConfig().eps_);
      break;
  }
}

}  // namespace datadriven
}  // namespace sgpp
