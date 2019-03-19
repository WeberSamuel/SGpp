% Copyright (C) 2008-today The SG++ project
% This file is part of the SG++ project. For conditions of distribution and
% use, please see the copyright notice provided with SG++ or at
% sgpp.sparsegrids.org

%% \page example_tutorial_m tutorial.m (Start Here)
%%
%% To be able to quickly start with a toolkit, it is often advantageous
%% (not only for the impatient users), to look at some code examples first.
%% In this tutorial, we give a short example program which interpolates a
%% bivariate function on a regular sparse grid.
%% Identical versions of the example are given in all languages
%% currently supported by SG++: C++, Python, Java, and MATLAB.
%%
%% In the example, we create a two-dimensional regular sparse grid of level 3
%% (with grid points \f$\vec{x}_j \in [0, 1]^2\f$)
%% using piecewise bilinear basis functions
%% \f$\varphi_j\colon [0, 1]^2 \to \mathbb{R}\f$.
%% We then interpolate the function
%%
%% \f[
%%   f\colon [0, 1]^2 \to \mathbb{R},\quad
%%   f(x_0, x_1) := 16 (x_0 - 1) x_0 (x_1 - 1) x_1
%% \f]
%%
%% with
%%
%% \f[
%%   u\colon [0, 1]^2 \to \mathbb{R},\quad
%%   u(x_0, x_1) := \sum_{j=0}^{N-1} \alpha_j \varphi_j(x_0, x_1)
%% \f]
%%
%% by calculating the coefficients \f$\alpha_j\f$ such that
%% \f$u(\vec{x}_j) = f(\vec{x}_j)\f$ for all \f$j\f$.
%% This process is called <i>hierarchization</i> in sparse grid contexts;
%% the \f$\alpha_j\f$ are called <i>(hierarchical) surpluses</i>.
%% Note that \f$f\f$ vanishes at the boundary of the domain \f$[0, 1]^2\f$;
%% therefore, we don't have to spend sparse grid points on the boundary.
%% Finally, we evaluate the sparse grid function \f$u\f$ at a point
%% \f$\vec{p} = (0.52, 0.73)\f$.
%%

%% Before starting, the function \f$f\f$, which we want to interpolate, is defined.
f = @(x0, x1) (16.0 * (x0 - 1.0) * x0 * (x1 - 1.0) * x1);

%% First, we create a two-dimensional grid (type sgpp::base::Grid)
%% with piecewise bilinear basis functions with the help of the factory method
%% sgpp::base::Grid.createLinearGrid().
dim = 2;
grid = sgpp.Grid.createLinearGrid(dim);

%% Then we obtain a reference to the grid's
%% sgpp::base::GridStorage object which allows us, e.g., to access grid
%% points, to obtain the dimensionality (which we print) and the
%% number of grid points.
gridStorage = grid.getStorage();
fprintf('dimensionality:         %u\n', gridStorage.getDimension());

%% Now, we use a sgpp::base::GridGenerator to
%% create a regular sparse grid of level 3.
%% Thus, \c gridStorage.getSize() returns 17, the number of grid points
%% of a two-dimensional regular sparse grid of level 3.
level = 3;
grid.getGenerator().regular(level);
fprintf('number of grid points:  %u\n', gridStorage.getSize());

%% We create an object of type sgpp::base::DataVector
%% which is essentially a wrapper around a \c double array.
%% The \c DataVector is initialized with as many
%% entries as there are grid points. It serves as a coefficient vector for the
%% sparse grid interpolant we want to construct. As the entries of a
%% freshly created \c DataVector are not initialized, we set them to
%% 0.0. (This is superfluous here as we initialize them in the
%% next few lines anyway.)
alpha = sgpp.DataVector(gridStorage.getSize());
alpha.setAll(0);
fprintf('length of alpha vector: %u\n', alpha.getSize());

%% The \c for loop iterates over all grid points: For each grid
%% point \c gp, the corresponding coefficient \f$\alpha_j\f$ is set to the
%% function value at the grid point's coordinates which are obtained by
%% \c getStandardCoordinate(dim).
%% The current coefficient vector is then printed.
for i = 0:gridStorage.getSize()-1
    gp = gridStorage.getPoint(i);
    alpha.set(i, f(gp.getStandardCoordinate(0), gp.getStandardCoordinate(1)));
end

fprintf('alpha before hierarchization: %s\n', char(alpha.toString()));

%% An object of sgpp::base::OperationHierarchisation is created and used to
%% hierarchize the coefficient vector, which we print.
sgpp.createOperationHierarchisation(grid).doHierarchisation(alpha);
fprintf('alpha after hierarchization:  %s\n', char(alpha.toString()));

%% Finally, a second \c DataVector is created which is used as a point to
%% evaluate the sparse grid function at. An object is obtained which
%% provides an evaluation operation (of type sgpp::base::OperationEvaluation),
%% and the sparse grid interpolant is evaluated at \f$\vec{p}\f$,
%% which is close to (but not exactly at) a grid point.
p = sgpp.DataVector(dim);
p.set(0, 0.52);
p.set(1, 0.73);
opEval = sgpp.createOperationEval(grid);
fprintf('u(0.52, 0.73) = %.4f\n', opEval.eval(alpha,p));

%% The example results in the following output:
%% \verbinclude tutorial.output.txt
%% It can be clearly seen that the surpluses decay with a factor of 1/4:
%% On the first level, we obtain 1, on the second 1/4, and on the third
%% 1/16 as surpluses.
