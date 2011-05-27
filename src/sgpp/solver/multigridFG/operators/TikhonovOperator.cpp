/*
 * TikhonovOperator.cpp
 *
 *  Created on: May 16, 2011
 *      Author: benk
 */

#include "TikhonovOperator.hpp"
#include <math.h>

using namespace combigrid;

TikhonovOperator::TikhonovOperator(const FullGridD* fg ,
		int nrInputPoints ,
		double lambda ,
		const std::vector<double>* xCoords ,
		const std::vector<double>* yCoords ) : OperatorFG(fg , 1) ,
		xCoords_(xCoords) , yCoords_(yCoords) {

	const int verb = 6;
	// the full grid must have boundary points
	// consistency check for the grid (boundary points, at least 3 points per axis)
	// - the grid must have a domain, even if it is a unit square
	COMBIGRID_ERROR_TEST( getFG()->getDomain() != NULL , " FULL grid must have a valid domain , even it is unit square");
	for (int d = 0 ; d < dim_ ; d++ ){
		COMBIGRID_ERROR_TEST( getFG()->getLevels()[d] >= 2 ,
				" Each dimension must have at least level 2 getFG()->getLevels()["<<d<<"]="<<getFG()->getLevels()[d]);
		COMBIGRID_ERROR_TEST( getFG()->returnBoundaryFlags()[d] == true , " The full grid must have in each dimension boundary points d="<<d);
	}


	nrRegPoints_ = nrInputPoints;
	dim_ = fg->getDimension();
	lambda_ = lambda;
	// the number of unknowns is the number of nodes in the full grids
	nrElem_ = fg->getNrElements();
	row_ptr_.resize(nrElem_+1,0);
	rhs_.resize(nrElem_,0.0);
	// first count the number of possible non zero elements of the
	nrMatrixElem_ = 0;
	COMBIGRID_OUT_LEVEL2( verb , "TikhonovOperator::TikhonovOperator count the number of possible non-zero matrix entries");
	int e=-2 , i=-2 , tmp = -2, d=-2 ;
	std::vector<int> axisIndex(dim_,0);
	for (e = 0 ; e < nrElem_ ; e++){
		tmp = e;
		// get the index for each axis
		for (i = dim_-1 ; i >= 0 ; i--) {
		   axisIndex[i] = tmp / (getFG()->getOffset(i));
		   tmp = tmp % getFG()->getOffset(i);
	    }
		// after we decomposed the index, we see how many overlaping
		tmp = 1;
		for (i = 0 ; i < dim_ ; i++){
			if ( (axisIndex[i] != 0) && (axisIndex[i] < getFG()->length(i)-1)){
				tmp = tmp * 3;
			} else {
				tmp = tmp * 2;
			}
		}
		nrMatrixElem_ = nrMatrixElem_ + tmp;
	}
	// now we know the number of possible non-zero elements in the matrix and so we can create the CRS structure
	matrixVal_.resize( nrMatrixElem_ , 0.0);
	col_ind_.resize( nrMatrixElem_ , -2);
	COMBIGRID_OUT_LEVEL2( verb , "TikhonovOperator::TikhonovOperator set up the CRS structure ");
	int actualP = 0 , j=0 , nrElPerRow = -2 , perm = (int)::pow(3,dim_);
	bool indFlag;
	std::vector<int> axisOffset(dim_,0);
	// loop over each node, to create first the CRS structure
	for (e = 0 ; e < nrElem_ ; e++)
	{
		tmp = e;
		// get the index for each axis
		for (i = dim_-1 ; i >= 0 ; i--) {
		   axisIndex[i] = tmp / (getFG()->getOffset(i));
		   tmp = tmp % getFG()->getOffset(i);
	    }
		row_ptr_[e] = actualP;
		COMBIGRID_OUT_LEVEL2( verb , " row_ptr_[" << e << "] = " << actualP );
		nrElPerRow = 0;
		// for each node we test each possible neighbor points (due to boundary not all might exist)
		for(i = 0 ; i < perm ; i++){
			tmp = i;
			// we decompose this in each direction
			for( j = dim_ -1 ; j >= 0 ; j--){
				axisOffset[j] = (tmp / 3) - 1;
				tmp = tmp % 3;
			}
			indFlag = true;
			for( j = 0 ; j < dim_ ; j++){
				indFlag = indFlag && ((axisIndex[j]+axisOffset[j] <= getFG()->length(j)-1) &&
							  (axisIndex[j]+axisOffset[j] >= 0));
			}
			// this index is ok, then get the linear index
			if (indFlag){
				tmp = 0;
				for( j = 0 ; j < dim_ ; j++){
					tmp = tmp + (axisIndex[j]+axisOffset[j])*getFG()->getOffset(j);
				}
				// increment the position counter
				col_ind_[actualP] = tmp;
				actualP = actualP + 1;
				nrElPerRow = nrElPerRow + 1;
				COMBIGRID_OUT_LEVEL2( verb , " i = " << i << " col_ind_["<<actualP<<"]=" << tmp );
			}
		}
	}
	// the last element is the end
	row_ptr_[nrElem_] = row_ptr_[nrElem_-1] + nrElPerRow;

	// now iterate over all regression points and fill the matrix and the right hand side
	std::vector<double> axisIntersect( dim_ ,0.0);
	std::vector<double> coordAct( dim_ ,0.0);
	std::vector<double> basisFuncVal( combigrid::powerOfTwo[dim_],0.0);
	std::vector<int>    basisIndex( combigrid::powerOfTwo[dim_] , -2 );
	int nrNode = combigrid::powerOfTwo[dim_] , startInd , offs ;
	int startSearch , endSearch;
	COMBIGRID_OUT_LEVEL2( verb , " Fill matrix and the right hand side ... ");
	for (e = 0 ; e < nrRegPoints_ ; e++){
		// find the cell, and get the intersection point, in each dimension
		startInd = 0; // the index of the 0th node in the found cell
		for (d = 0 ; d < dim_ ; d++){
			coordAct[d] = (*xCoords)[e*dim_ + d];
			getFG()->getDomain()->get1DDomain(d).findEntry( coordAct[d] , getFG()->getLevels()[d] , axisIndex[d] , axisIntersect[d] );
			// the linear index of the first node
			startInd = startInd + getFG()->getOffset(d)*axisIndex[d];
		}

		// compute all 2^D basis function values, and the index
		for (i = 0 ; i < nrNode ; i++) {
			basisFuncVal[i] = 1.0;
			basisIndex[i] = startInd;
			tmp = i;
			for ( d = 0 ; d < dim_ ; d++ ){
				// get the last bit
				offs = tmp % 2;
				tmp = tmp >> 1;
				//basis function computation
				basisFuncVal[i] = (double)(1-offs)*basisFuncVal[i]*getFG()->getBasisFct()->functionEval1(axisIntersect[d]) +
				 (double)(offs)*basisFuncVal[i]*getFG()->getBasisFct()->functionEval2(axisIntersect[d]);
				//add to the basis function if the bit is one
				basisIndex[i] = basisIndex[i] + offs*getFG()->getOffset(d);
			}
			COMBIGRID_OUT_LEVEL2( verb , "basisFuncVal[" << i << "]=" << basisFuncVal[i] );
			COMBIGRID_OUT_LEVEL2( verb , "basisIndex[" << i << "]=" << basisIndex[i] );
		}

		// B*BT to all (i,j)
		for (i = 0 ; i < nrNode ; i++) {
			// add i to the right hand side
			COMBIGRID_OUT_LEVEL2( verb , " add to rhs_[ " <<  basisIndex[i] << " ] ");
			rhs_[ basisIndex[i] ] = rhs_[ basisIndex[i] ] + (1.0/(double)(nrRegPoints_))*basisFuncVal[i];
			// here add B*BT to all (i,j)
			for (j = 0 ; j < nrNode ; j++) {
				// look in row basisIndex[i] for basisIndex[j]
				startSearch = row_ptr_[ basisIndex[i] ];
				endSearch = row_ptr_[ basisIndex[i] + 1];
				tmp = -2;
				for ( ; startSearch <= endSearch ; startSearch++){
					if ( col_ind_[startSearch] == basisFuncVal[j])
						{ tmp = row_ptr_[ basisIndex[i] ] + (endSearch - startSearch); }
				}
				COMBIGRID_OUT_LEVEL2( verb , " add to B*B^T(" << i << "," << j <<")");
				// add this entry to the matrix
				matrixVal_[tmp] = matrixVal_[tmp] + (1.0/(double)(nrRegPoints_))*basisFuncVal[i]*basisFuncVal[j];
			}
		}
	}
	// finally add -lambda*C to the matrix (for each node), but not to those which are at the boundary
	// (i,i) and (i,i-+offs)
	int p_e , p_e_m , p_e_p ; // the positions of the nodes
	double h0 = 1.0 , h1 = 1.0;
	COMBIGRID_OUT_LEVEL2( verb , "add the lambda*C part");
	for ( e = 0 ; e < nrElem_ ; e++)
	{
			// here we look for the BBT(i,i) element,
			startSearch = row_ptr_[ e ];
			endSearch = row_ptr_[ e + 1];
			p_e = -2;
			for ( ; startSearch <= endSearch ; startSearch++){
				if ( col_ind_[startSearch] == e)
					{ p_e = row_ptr_[ basisIndex[i] ] + (endSearch - startSearch); }
			}

			COMBIGRID_OUT_LEVEL2( verb , "element position e="<<e << " p_e="<<p_e);
		    for (i = dim_ - 1 ; i >= 0 ; i--)
		    {
			   axisIndex[i] = tmp / (getFG()->getOffset(i));
			   tmp = tmp % getFG()->getOffset(i);
			   offs = getFG()->getOffset(i);
			   // add to the direction only if it is not boundary direction
			   if ( (axisIndex[i] > 0) && ( axisIndex[i] < getFG()->length(i)-1 )){
				    // look for e - getFG()->getOffset(i) and for e+ getFG()->getOffset(i) elements
					startSearch = row_ptr_[ e - offs ];
					endSearch = row_ptr_[ e - offs + 1];
					p_e_m = -2;
					for ( ; startSearch <= endSearch ; startSearch++){
						if ( col_ind_[startSearch] == e - offs)
							{ p_e_m = row_ptr_[ basisIndex[i] ] + (endSearch - startSearch); }
					}
					startSearch = row_ptr_[ e + offs ];
					endSearch = row_ptr_[ e + offs + 1];
					p_e_p = -2;
					for ( ; startSearch <= endSearch ; startSearch++){
						if ( col_ind_[startSearch] == e + offs)
							{ p_e_p = row_ptr_[ basisIndex[i] ] + (endSearch - startSearch); }
					}
					// get h0 and h1
					// h0 = S_scaling(1,i) - S_scaling(1,i-1);
			    	// h1 = S_scaling(1,i+1) - S_scaling(1,i);
					getFG()->getDomain()->get1DDomain(d).getMeshWidth( axisIndex[i] , getFG()->getLevels()[i] , h0 , h1 );
					//	% we need  (- laplace F) * lambda
					//	C(ind,ind-nr_points(2)) = C(ind,ind-nr_points(2)) - h1/(0.5*h0*h1*(h0+h1));
					//	C(ind,ind) = C(ind,ind) + (h0+h1)/(0.5*h0*h1*(h0+h1));
					//	C(ind,ind+nr_points(2)) = C(ind,ind+nr_points(2)) - h0/(0.5*h0*h1*(h0+h1));
					matrixVal_[p_e_m] = matrixVal_[p_e_m] - h1/(0.5*h0*h1*(h0+h1));
					matrixVal_[p_e]   = matrixVal_[p_e] + (h0+h1)/(0.5*h0*h1*(h0+h1));
					matrixVal_[p_e_p] = matrixVal_[p_e_p] - h0/(0.5*h0*h1*(h0+h1));
			   }
		   }
	}
}

TikhonovOperator::~TikhonovOperator() {
	// nothing to do, no object created on the heap
}

OperatorFG* TikhonovOperator::factory(const FullGridD* fg) const {

	return (new TikhonovOperator( fg , nrRegPoints_ , lambda_ , xCoords_ , yCoords_ ));

}

void TikhonovOperator::getRHS(std::vector<double>& rhs , int& nrSpace) const {
	nrSpace = this->getNrSpace();
	rhs.resize(nrElem_);
	COMBIGRID_OUT_LEVEL2( 6 , "TikhonovOperator::getRHS ... START");
	for (int i = 0 ; i < nrElem_ ; i++)  { rhs[i] = rhs_[i]; }
	COMBIGRID_OUT_LEVEL2( 6 , "TikhonovOperator::getRHS ... END");
}

void TikhonovOperator::multiplyVector(std::vector<double>& inVect , std::vector<double>& outVect) const {
	// implement this
	int i , of , verb = 6;
	double tmp = 0.0;
	COMBIGRID_OUT_LEVEL2( verb , "TikhonovOperator::multiplyVector ... START");
	for (i = 0 ; i < nrElem_ ; i++){
		tmp = 0.0;
		for ( of = row_ptr_[ i ] ; of < row_ptr_[ i + 1] ; of++ ){
			tmp = tmp + matrixVal_[ of ] * inVect[ col_ind_[of] ];
		}
		outVect[i] = tmp;
	}
	COMBIGRID_OUT_LEVEL2( verb , "TikhonovOperator::multiplyVector ... END");
}

void TikhonovOperator::doSmoothing(int nrIt ,
		std::vector<double>& u, std::vector<double>& rhs) const {
	// make a simple Gauss-Seidel
	int i , of , aii , verb = 6;
	double tmp = 0.0;
	COMBIGRID_OUT_LEVEL2( verb , "TikhonovOperator::doSmoothing ... START");
	for (i = 0 ; i < nrElem_ ; i++)
	{
		tmp = 0.0;
		aii = -2;
		for ( of = row_ptr_[i] ; of < row_ptr_[i + 1] ; of++ )
		{
			tmp = tmp + matrixVal_[of] * u[col_ind_[of]];
			if ( col_ind_[of] == i ) { aii = of; }
		}
		// because this matrix is symmetric
		u[i] = ( 1.0/matrixVal_[aii] ) * ( rhs[i] - tmp + u[i]*matrixVal_[aii] );
	}
	COMBIGRID_OUT_LEVEL2( verb , "TikhonovOperator::doSmoothing ... END");
}

void TikhonovOperator::setNewLambda(double lambda){
	// todo: implement this
	// reinitialize the matrix by subtracting the old_lambda*C and then adding the new_lambda*C
}
