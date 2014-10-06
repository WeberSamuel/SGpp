/******************************************************************************
 * Copyright (C) 2013 Technische Universitaet Muenchen                         *
 * This file is part of the SG++ project. For conditions of distribution and   *
 * use, please see the copyright notice at http://www5.in.tum.de/SGpp          *
 ******************************************************************************/
//@author Michael Lettrich (m.lettrich@mytum.de), Maxim Schmidt (maxim.schmidt@tum.de)
#ifndef ONLINEPREDICTIVEREFINEMENTDIMENSION_HPP_
#define ONLINEPREDICTIVEREFINEMENTDIMENSION_HPP_

#include "RefinementDecorator.hpp"
#include "base/grid/generation/hashmap/AbstractRefinement.hpp"
#include "base/grid/generation/functors/PredictiveRefinementDimensionIndicator.hpp"
#include <vector>
#include <utility>
#include "sgpp_base.hpp"
#include "sgpp_datadriven.hpp"
#include "base/exception/application_exception.hpp"

namespace sg {
namespace base {


/*
 * PredictiveRefinement performs local adaptive refinement of a sparse grid using the PredictiveRefinementIndicator.
 * This way, instead of surpluses that are most often used for refinement, refinement decisions are based upon an estimation
 * to the contribution of the MSE, which is especially helpfull for regression.
 *
 */
class OnlinePredictiveRefinementDimension: public virtual RefinementDecorator {
	friend class LearnerOnlineSGD;
public:

  typedef std::pair<size_t, size_t> key_type; // gred point seq number and dimension
  typedef double value_type; // refinement functor value

  typedef std::pair<size_t, unsigned int> refinement_key;
  typedef std::map<refinement_key, double> refinement_map;

	OnlinePredictiveRefinementDimension(AbstractRefinement* refinement): RefinementDecorator(refinement), iThreshold_(0.0){};
  void free_refine(GridStorage* storage, PredictiveRefinementDimensionIndicator* functor);



	/**
	 * Examines the grid points and stores those indices that can be refined
	 * and have maximal indicator values.
	 *
	 * @param storage hashmap that stores the grid points
	 * @param functor a PredictiveRefinementIndicator specifying the refinement criteria
	 * @param refinements_num number of points to refine
	 * @param max_indices the array where the point indices should be stored
	 * @param max_values the array where the corresponding indicator values
	 * should be stored
	 */
	virtual void collectRefinablePoints(GridStorage* storage,
				size_t refinements_num, refinement_map* result);

	bool hasLeftChild(GridStorage* storage, GridIndex* gridIndex, size_t dim);
	bool hasRightChild(GridStorage* storage, GridIndex* gridIndex, size_t dim);

	void setTrainDataset(DataMatrix* trainDataset_);
	void setClasses(DataVector* classes_);
	void setErrors(DataVector* errors_);

	// For the Python test case
	double basisFunctionEvalHelper(unsigned int level, unsigned int index, double value);

protected:
	virtual void refineGridpointsCollection(GridStorage* storage,
	    RefinementFunctor* functor, size_t refinements_num, size_t* max_indices,
	    PredictiveRefinementDimensionIndicator::value_type* max_values);

	virtual size_t getIndexOfMin(PredictiveRefinementDimensionIndicator::value_type* array,
	    size_t length);

private:
	double iThreshold_;
	std::map<key_type, value_type> refinementCollection_;
	//virtual static bool refinementPairCompare(std::pair<key_type, value_type>& firstEl, std::pair<key_type, value_type>& secondEl);

	DataMatrix* trainDataset;
	DataVector* classes;
	DataVector* errors;

};

} /* namespace base */
} /* namespace sg */
#endif /* ONLINEPREDICTIVEREFINEMENTDIMENSION_HPP_ */
