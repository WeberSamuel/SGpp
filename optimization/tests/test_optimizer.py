# Copyright (C) 2008-today The SG++ project
# This file is part of the SG++ project. For conditions of distribution and
# use, please see the copyright notice provided with SG++ or at
# sgpp.sparsegrids.org

import unittest
import pysgpp
import objective_functions
import random
import math

class TestOptimizer(unittest.TestCase):
    def setUp(self):
        """Initialize the test case."""
        # disable status output
        pysgpp.cvar.OptPrinterInstance.setVerbosity(-1)
        # disable multi-threading
        pysgpp.omp_set_num_threads(1)
    
    def testUnconstrainedOptimizers(self):
        """Test unconstrained optimizers in SGPP::optimization::optimizer."""
        f = objective_functions.ExampleFunction()
        f_gradient = objective_functions.ExampleFunctionGradient()
        f_hessian = objective_functions.ExampleFunctionHessian()
        
        # Test All The Optimizers!
        N = 1000
        optimizers = [pysgpp.OptGradientDescent(f, f_gradient, N),
                      pysgpp.OptNLCG(f, f_gradient, N),
                      pysgpp.OptNewton(f, f_hessian, N),
                      pysgpp.OptAdaptiveGradientDescent(f, f_gradient, N),
                      pysgpp.OptAdaptiveNewton(f, f_hessian, N),
                      pysgpp.OptBFGS(f, f_gradient, N),
                      pysgpp.OptRprop(f, f_gradient, N),
                      pysgpp.OptNelderMead(f, N),
                      pysgpp.OptMultiStart(f, N),
                      pysgpp.OptDifferentialEvolution(f, N)]
        
        for optimizer in optimizers:
            xopt = pysgpp.DataVector(0)
            # set starting point
            x0 = pysgpp.DataVector(2)
            x0[0] = 0.8
            x0[1] = 0.5
            optimizer.setStartingPoint(x0)
            # optimize
            fopt = optimizer.optimize(xopt)
            print optimizer
            print xopt
            print fopt
            
            # test xopt and fopt
            self.assertEqual(len(xopt), 2)
            self.assertAlmostEqual(xopt[0], 3.0/16.0 * math.pi, places=3)
            self.assertAlmostEqual(xopt[1], 3.0/14.0 * math.pi, places=3)
            self.assertAlmostEqual(fopt, -2.0, places=6)
    
    def testConstrainedOptimizers(self):
        """Test constrained optimizers in SGPP::optimization::optimizer."""
        # TODO
        pass
