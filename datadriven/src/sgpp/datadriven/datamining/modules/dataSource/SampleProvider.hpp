/* Copyright (C) 2008-today The SG++ project
 * This file is part of the SG++ project. For conditions of distribution and
 * use, please see the copyright notice provided with SG++ or at
 * sgpp.sparsegrids.org
 *
 * SampleProvider.hpp
 *
 *  Created on: Feb 8, 2016
 *      Author: perun, Michael Lettrich
 */

#pragma once

#include <sgpp/datadriven/tools/Dataset.hpp>
#include <sgpp/globaldef.hpp>

namespace sgpp {
namespace datadriven {

/**
 * SampleProvider is an abstraction for object that provide sample data from various sources for
 * example datasets from files (ARFF, CSV) or synthetic datasets generated by sampling functions (
 * Friedmann datasets).
 */
class SampleProvider {
 public:
  // TODO(lettrich): Ignore unrecognized methods in swig
  SampleProvider() = default;
  SampleProvider(const SampleProvider& rhs) = default;
  SampleProvider(SampleProvider&& rhs) = default;
  SampleProvider& operator=(const SampleProvider& rhs) = default;
  SampleProvider& operator=(SampleProvider&& rhs) = default;
  virtual ~SampleProvider() = default;

  /**
   * Clone pattern for polymorphic cloning (mainly interresting for copy constructors).
   *
   * @return #sgpp::datadriven::SampleProvider* pointer to clone of this class. This object is owned
   * by the caller.
   */
  virtual SampleProvider* clone() const = 0;

  /**
   * Lets the user request a certain amount of samples. This functionality is is designed for
   * streaming algorithms where data is processed in batches.
   * @param howMany number requested amount of samples. The amount of actually provided samples can
   * be smaller, if there is not sufficient data.
   * @return #sgpp::datadriven::Dataset* Pointer to a new #sgpp::datadriven::Dataset object
   * containing at most the requested amount of
   * samples. This object is owned by the caller.
   */
  virtual Dataset* getNextSamples(size_t howMany) = 0;

  /**
   * Asks to return all available samples. This functionality is designed for returning all
   * available samples from an entire file.
   * @return #sgpp::datadriven::Dataset* Pointer to a new #sgpp::datadriven::Dataset object. This
   * object is owned by the caller.
   */
  virtual Dataset* getAllSamples() = 0;

  /**
   * Returns the maximal dimensionality of the data.
   * @return dimensionality of the #sgpp::datadriven::Dataset.
   */
  virtual size_t getDim() const = 0;
};
}
}
