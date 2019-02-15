from argparse import ArgumentParser
from mpl_toolkits.mplot3d import Axes3D
import os
import time

from matplotlib import cm

import active_subspaces as ac
import asFunctions
import cPickle as pickle
import matplotlib.pyplot as plt
import numpy as np
import pandas as pn
import pysgpp
import quasiMCIntegral


def dataVectorToPy(v):
    return np.fromstring(v.toString()[1:-2], dtype=float, sep=',')


# SG++ AS functionalities return eigenvalues as increasingly sorted DataVector.
# reverse the order and cast to numpy.ndarray
def reverseDataVectorToNdArray(v):
    n = np.ndarray(v.getSize())
    for i in range(v.getSize()):
        n[-i - 1] = v[i]
    return n


# SG++ AS functionalities return eigenvectors as increasingly sorted (by eigenvalues)
# DataMatrix. reverse the order and cast to numpy.ndarray
def reverseDataMatrixToNdArray(m):
    n = np.ndarray(shape=(m.getNrows(), m.getNcols()))
    for i in range(m.getNcols()):
        for j in range(m.getNrows()):
            n[i, -j - 1] = m.get(i, j)
    return n


# wraps the objective function for SGpp    
class objFuncSGpp(pysgpp.OptScalarFunction):

    def __init__(self, objFunc):
        self.numDim = objFunc.getDim()
        self.objFunc = objFunc
        super(objFuncSGpp, self).__init__(self.numDim)

    def eval(self, v):
        x = np.ndarray(shape=(1, self.numDim))
        for i in range(self.numDim):
            x[0, i] = v[i]
        return  self.objFunc.eval(x)[0][0]
    
    def getDim(self):
        return self.numDim

    
# creates data from analytical function to test datadriven approaches    
def createData(f, numSamples, pointsPath, valuesPath):
    generalPath = os.path.dirname(pointsPath)
    print("creating data and saving it to {}".format(generalPath))
    numDim = f.getDim()
    randomPoints = np.random.rand(numDim, numSamples)
    evaluationPoints = pysgpp.DataMatrix(numDim, numSamples)
    for i in range(numDim):
        for j in range(numSamples):
            evaluationPoints.set(i, j, randomPoints[i, j])
    functionValues = pysgpp.DataVector(numSamples)
    for i in range(numSamples):
        functionValues[i] = f.eval(randomPoints[:, i])
    if not os.path.exists(generalPath):
        os.makedirs(generalPath)
    evaluationPoints.toFile(pointsPath)
    functionValues.toFile(valuesPath)
    return evaluationPoints, functionValues


def getData(savePath, numDataPoints, f, method, model='SingleDiode'):
    if model == 'SingleDiode':
        df = pn.DataFrame.from_csv('/home/rehmemk/git/SGpp/activeSubSpaces/results/SingleDiode/data/SingleDiodePV-Pmax.txt')
        data = df.values
        X = data[:, :5]
        f = data[:, 5]
        df = data[:, 6:]
#         labels = df.keys()
#         in_labels = labels[:5]
#         out_label = labels[5]
        # Normalize the inputs to the interval [-1,1] / [0,1]. The second variable is uniform in the log space.
        xl = np.array([0.05989, -24.539978662570231, 1.0, 0.16625, 93.75])
        xu = np.array([0.23598, -15.3296382905940, 2.0, 0.665, 375.0])
        Y = X.copy()
        Y[:, 1] = np.log(Y[:, 1])
        if method in ['asSGpp']:
            XX = asFunctions.unnormalize(Y, 0, 1, xl, xu)
        elif method in ["QPHD"]:
            XX = ac.utils.misc.BoundedNormalizer(xl, xu).normalize(Y)
        evaluationPoints = pysgpp.DataMatrix(XX[0:numDataPoints, :])
        functionValues = pysgpp.DataVector(f[0:numDataPoints])
        
        evaluationPoints.transpose()
    else:
        generalPath = os.path.dirname(savePath)
        pointsPath = os.path.join(generalPath, 'data', 'dataPoints' + str(numDataPoints) + '.dat')
        valuesPath = os.path.join(generalPath, 'data', 'dataValues' + str(numDataPoints) + '.dat')
        if os.path.exists(pointsPath) and os.path.exists(valuesPath):
            evaluationPoints = pysgpp.DataMatrix.fromFile(pointsPath)
            functionValues = pysgpp.DataVector.fromFile(valuesPath)
        else:
            evaluationPoints, functionValues = createData(f, numDataPoints, pointsPath, valuesPath)
    
    # split data 80/20 into training data and validation data
    splitIndex = numDataPoints * 8 / 10
    numDim = evaluationPoints.getNrows()
    trainingValues = pysgpp.DataVector(splitIndex)
    trainingPoints = pysgpp.DataMatrix(numDim, splitIndex)
    validationValues = pysgpp.DataVector(numDataPoints - splitIndex)
    validationPoints = pysgpp.DataMatrix(numDim, numDataPoints - splitIndex)
    for i in range(splitIndex):
        trainingValues[i] = functionValues[i]
        for j in range(numDim):
            trainingPoints.set(j, i, evaluationPoints.get(j, i))
    for i in range(numDataPoints - splitIndex):
        validationValues[i] = functionValues[splitIndex + i]
        for j in range(numDim):
            validationPoints.set(j, i, evaluationPoints.get(j, splitIndex + i))
            
    return trainingPoints, trainingValues, validationPoints, validationValues

    
# uniformly distributed points in numDim dimensions     
def uniformX(numSamples, numDim, l=-1, r=1):
    x = np.ndarray(shape=(numSamples, numDim))
    for d in range(numDim):
        x[:, d] = np.random.uniform(l, r, (numSamples, 1))[:, 0]
    return x


# points distributed according to the borehole example
def boreholeX(numSamples):
    rw = np.random.normal(.1, .0161812, (numSamples, 1))
    r = np.exp(np.random.normal(7.71, 1.0056, (numSamples, 1)))
    Tu = np.random.uniform(63070, 115600, (numSamples, 1))
    Hu = np.random.uniform(990, 1110, (numSamples, 1))
    Tl = np.random.uniform(63.1, 116, (numSamples, 1))
    Hl = np.random.uniform(700, 820, (numSamples, 1))
    L = np.random.uniform(1120, 1680, (numSamples, 1))
    Kw = np.random.uniform(9855, 12045, (numSamples, 1))
    x_t = np.hstack((rw, r, Tu, Hu, Tl, Hl, L, Kw))
    xl = np.array([63070, 990, 63.1, 700, 1120, 9855])
    xu = np.array([115600, 1110, 116, 820, 1680, 12045])
    # XX = normalized input matrix
    XX = ac.utils.misc.BoundedNormalizer(xl, xu).normalize(x_t[:, 2:])
    # normalize non-uniform inputs
    rw_norm = ((rw - .1) / .0161812).reshape(numSamples, 1)
    r_norm = np.log(r); r_norm = ((r_norm - 7.71) / 1.0056).reshape(numSamples, 1)
    XX = np.hstack((rw_norm, r_norm, XX))
    return XX


#---------------------usual SGpp interpolant----------------------------
# objFunc        objective function
# gridType       type of basis functions
# degree         degree of basis functions
# numResponse    number of points (adaptive) or level (regular) of the response surface
# responseType   method for creation of the response surface. Adaptive or regular
# numerrorPoints number of MC points used to calculate the l2 interpolation error
#-----------------------------------------------------------------------
def SGpp(objFunc, gridType, degree, numResponse, model, responseType, numErrorPoints=10000, savePath=None, numDataPoints=10000,
                numRefine=10, initialLevel=2):
    print("\nnumGridPoints = {}".format(numResponse))
    pysgpp.OptPrinter.getInstance().setVerbosity(-1)
    numDim = objFunc.getDim()
    f = objFuncSGpp(objFunc)
    
    sparseResponseSurf = pysgpp.SparseGridResponseSurfaceNakBspline(f, \
                                                                    pysgpp.Grid.stringToGridType(gridType), degree)
    if responseType == 'adaptive':
        initialLevel = 1
        sparseResponseSurf.createSurplusAdaptiveResponseSurface(numResponse, initialLevel, numRefine)
        l2Error = sparseResponseSurf.l2Error(f, numErrorPoints)
    elif responseType == 'regular':
        print("TODO: Write routien to guess level from numResponse!")
        # sparseResponseSurf.createRegularResponseSurface(numResponse)
        # l2Error = sparseResponseSurf.l2Error(f, numErrorPoints)
    elif responseType == 'dataR':
        print("TODO: Write routien to guess level from numResponse!")
        trainingPoints, trainingValues, validationPoints, validationValues = getData(savePath, numDataPoints, f, 'SGpp', model)
        responseLambda = 1e-6
        responseLevel = 4  
        if objFunc.getDim() == 5:
            responseLevel = 1
            if numResponse >= 11:
                responseLevel = 2
            if numResponse >= 71:
                responseLevel = 3
            if numResponse >= 351:
                responseLevel = 4
            if numResponse >= 1471:
                responseLevel = 5
            if numResponse > 5503:
                responseLevel = 6
            if numResponse >= 18943:
                responseLevel = 7
        print('{} gridpoints requested, using level {} (This is hard coded, find a way to determine level!)) '.format(numResponse, responseLevel))
        sparseResponseSurf.createRegularResponseSurfaceData(responseLevel, trainingPoints, trainingValues, responseLambda)
        numValidationPoints = validationValues.getSize()
        validationEval = pysgpp.DataVector(numValidationPoints)
        validationPoint = pysgpp.DataVector(numDim) 
        for i in range(numValidationPoints):
            validationPoints.getColumn(i, validationPoint)
            validationEval[i] = sparseResponseSurf.eval(validationPoint)
        validationEval.sub(validationValues)
        l2Error = validationEval.l2Norm() / numValidationPoints
        
    print("sparse interpolation error {}".format(l2Error))
    lb, ub = objFunc.getDomain()
    vol = np.prod(ub - lb)
    integral = sparseResponseSurf.getIntegral() * vol
    integralError = abs(integral - objFunc.getIntegral())
    # print("sparse integral: {}".format(integral))
    print("sparse integral error {}\n".format(integralError))
    numGridPoints = sparseResponseSurf.getCoefficients().getSize()
    print('actual num grid points = {}'.format(numGridPoints))
    
    return l2Error, integral, integralError, numGridPoints


def integrateResponseSurface(responseSurf, integralType, objFunc, quadOrder=7, approxLevel=8, approxDegree=3, numHistogramMCPoints=1000000):
    lb, ub = objFunc.getDomain()
    vol = np.prod(ub - lb)
    integral = float('NaN')
    if integralType == 'MC':
        integral = responseSurf.getMCIntegral(100, numHistogramMCPoints, 'Halton') * vol
    elif integralType == 'Hist':
        integral = responseSurf.getHistogramBasedIntegral(11, numHistogramMCPoints, 'Halton') * vol
    elif integralType == 'Spline':
        integral = responseSurf.getSplineBasedIntegral(quadOrder) * vol
    elif integralType == 'appSpline':
        integral = responseSurf.getApproximateSplineBasedIntegral(approxLevel, approxDegree) * vol
    integralError = abs(integral - objFunc.getIntegral())
    return integral, integralError


def asRecognition(asmType, f, gridType, degree, numASM, initialLevel, numRefine, savePath, numDataPoints, model, printFlag):
    datatypes = ['data', 'datadriven', 'dataR', 'datadrivenR']
    validationValues = []
    validationPoints = []
    if asmType in ["adaptive", "regular"]:
        ASM = pysgpp.ASMatrixBsplineAnalytic(f, pysgpp.Grid.stringToGridType(gridType), degree)
        if asmType == 'adaptive':
            ASM.buildAdaptiveInterpolant(numASM, initialLevel, numRefine)
        elif asmType == 'regular':
            print("numASM: {}".format(numASM))
            ASM.buildRegularInterpolant(numASM)
    elif asmType in datatypes:
        trainingPoints, trainingValues, validationPoints, validationValues = getData(savePath, numDataPoints, f, 'asSGpp', model)
        ASM = pysgpp.ASMatrixBsplineData(trainingPoints, trainingValues, pysgpp.Grid.stringToGridType(gridType), degree)
        if asmType == 'data':
            ASM.buildAdaptiveInterpolant(numASM)
        elif asmType == 'dataR':
            print("\n         TODO: asRecognition asmLevel in case dataR is hard coded and not automatically chosen!")
            asmLevel = 4
            ASM.buildRegularInterpolant(asmLevel)
            print("         used asmLevel={}, resulting in {} grid  points\n".format(asmLevel, ASM.getCoefficients().getSize()))
        else:
            print("asmType not supported currently")
    
    if savePath is not None:
        if not os.path.exists(savePath):
            os.makedirs(savePath)
        ASM.toFile(savePath)
 
    ASM.createMatrixGauss()
    ASM.evDecompositionForSymmetricMatrices()
    eivalSGpp = ASM.getEigenvaluesDataVector();    
    eivecSGpp = ASM.getEigenvectorsDataMatrix()
    eival = reverseDataVectorToNdArray(eivalSGpp);   
    eivec = reverseDataMatrixToNdArray(eivecSGpp)
    
    if printFlag == 1 and asmType in ["adaptive", "regular"]:
        print("error in ASM interpolant: {}".format(ASM.l2InterpolationError(10000)))
        print("number of ASM interpolation points: {}".format(ASM.getCoefficients().getSize()))
    
    return ASM, eival, eivec, validationValues, validationPoints


#---------------------SGpp with active subspaces----------------------------
# objFunc        the objective Function
# gridType       type of the basis functions
# degree         degree of the basis functions
# numASM         number of points (adaptive) or level (regular) for the creation of the ASM
# numRepsonse    number of points (adaptive) or level (regular,detection) of the response surface
# asmType        method for creation of the ASM (adaptive or regular)
# responseType   method for creation of the response surface. Adaptive, regular or from the detection points
# integralType   Monte Carlo integral ('MC') or semi continuous integral ('Cont')
# numErrorPoints number of MC points used to calculate the l2 interpolation error
# savePath       path to save the interpolation grid and coefficients to or None
#--------------------------------------------------------------------------
def SGppAS(objFunc, gridType, degree, numASM, numResponse, model, asmType='adaptive',
                responseType='adaptive', integralType='Hist', numErrorPoints=10000,
                numHistogramMCPoints=1000000, savePath=None, approxLevel=8,
                approxDegree=3, numShadow1DPoints=0, numDataPoints=10000, printFlag=1,
                numRefine=10, initialLevel=2, doResponse=1, doIntegral=1):
    
    # dummy values if response surface or integral are not calculated
    l2Error, integral, integralError = 0, 0, 0
    shadow1DEvaluations, bounds, responseCoefficients = np.zeros(numShadow1DPoints), [0, 0], []
    
    print("\nnumGridPoints = {}".format(numResponse))
    if responseType in ['regular', 'dataR', 'datadrivenR']:
        responseLevel = int(np.math.log(numResponse + 1, 2)) 
        asmLevel = responseLevel
        numASM = asmLevel  # wrapper
        print("using level {} which has {} points for regular grids".format(responseLevel, 2 ** responseLevel - 1))
    print("numDataPoints = {}".format(numDataPoints))
    datatypes = ['data', 'datadriven', 'dataR', 'datadrivenR']
    
    pysgpp.OptPrinter.getInstance().setVerbosity(-1)
    numDim = objFunc.getDim()
    f = objFuncSGpp(objFunc)
    
    start = time.time()
    ASM, eival, eivec, validationValues, validationPoints = asRecognition(asmType, f, gridType, degree, numASM, initialLevel,
                                                                          numRefine, savePath, numDataPoints, model, printFlag)
    recognitionTime = time.time() - start; start = time.time()
    if printFlag == 1:
        print("recognition time:               {}".format(recognitionTime))
    
    print(eival)
    #     print(eivec)
    print("first eigenvector {}".format(eivec[:, 0]))
    
    n = 1  # active subspace identifier
    responseDegree = degree  
    responseGridType = pysgpp.GridType_NakBsplineBoundary  
    responseSurf = ASM.getResponseSurfaceInstance(n, responseGridType, responseDegree)
    if responseType == 'adaptive':
        responseSurf.createAdaptiveReducedSurfaceWithPseudoInverse(numResponse, f, initialLevel, numRefine)
    elif responseType == 'regular':
        responseSurf.createRegularReducedSurfaceWithPseudoInverse(responseLevel, f)
    elif responseType in ['data', 'datadriven', 'dataR', 'datadrivenR']:
        asmPoints = ASM.getEvaluationPoints()
        asmValues = ASM.getFunctionValues()
        responseLambda = 1e-8
        if responseType == 'data':
            responseSurf.createAdaptiveReducedSurfaceFromData(numResponse, asmPoints, asmValues, initialLevel, numRefine, responseLambda)
        elif responseType == 'dataR':
            responseSurf.createRegularReducedSurfaceFromData(asmPoints, asmValues, responseLevel, responseLambda)
        elif responseType == 'datadrivenR':
            responseSurf.createRegularReducedSurfaceFromData_DataDriven(asmPoints, asmValues, responseLevel, responseLambda)
    numGridPoints = responseSurf.getCoefficients().getSize()

    if doResponse == 1:
        responseCreationTime = time.time() - start
        if printFlag == 1:
            print("response surface creation time: {}".format(responseCreationTime))
    
        bounds = responseSurf.getBounds() 
        print("leftBound: {} rightBound: {}".format(bounds[0], bounds[1]))
        
        if responseType not in datatypes:
            errorPoints = np.random.random((numErrorPoints, objFunc.getDim()))
            validationValues = objFunc.eval(errorPoints)
            validationPoints = pysgpp.DataMatrix(errorPoints)
            validationPoints.transpose()
        numValidationPoints = validationPoints.getNcols()
        responseEval = np.ndarray((numValidationPoints, 1))
        validationPoint = pysgpp.DataVector(numDim) 
        for i in range(numValidationPoints):
            validationPoints.getColumn(i, validationPoint)
            responseEval[i] = responseSurf.eval(validationPoint)
        l2Error = np.linalg.norm(responseEval - validationValues) / numValidationPoints
        print("interpol error: {}".format(l2Error))
        # print("Comparison: l2 error {}".format(responseSurf.l2Error(f, numErrorPoints)))
    
#     W1 = eivec[:, 0]
#     W1 = [1 / np.sqrt(5)] * 5
#     plt.scatter(errorPoints.dot(W1), responseEval, label='approx')
#     plt.scatter(errorPoints.dot(W1), validationValues, label='f')
#     plt.legend()
#     plt.show()
    
        if numShadow1DPoints > 0:
            X1unit = np.linspace(0, 1, numShadow1DPoints)
            shadow1DEvaluations = [responseSurf.eval1D(x)  for x in X1unit]

    if doIntegral == 1:
        quadOrder = 7
        start = time.time()
        integral, integralError = integrateResponseSurface(responseSurf, integralType, objFunc, quadOrder, approxLevel, approxDegree, numHistogramMCPoints)
        integrationTime = time.time() - start; 
        if printFlag == 1:
            print("integration time:               {}".format(integrationTime))
    #     print("integral: {}\n".format(integral)),
        print("integral error: {}".format(integralError))
    
# plot interpolant of 2D function
#     X, Y = np.meshgrid(np.linspace(0, 1, 100), np.linspace(0, 1, 100))
#     I = np.zeros(np.shape(X))
#     F = np.ndarray(np.shape(X))
#     for i in range(len(X)):
#         for j in range(len(X[0])):
#             I[i, j] = responseSurf.eval(pysgpp.DataVector([X[i, j], Y[i, j]]))
#             F[i, j] = f.eval([X[i, j], Y[i, j]])
#     fig = plt.figure(); ax = fig.gca(projection='3d')
#     ax.plot_surface(X, Y, I, cmap=cm.viridis, linewidth=0, antialiased=False)
#     ax.plot_wireframe(X, Y, F, rstride=10, cstride=10,color='r')
#     plt.show()

    responseGrid = responseSurf.getGrid()
    responseCoefficients = responseSurf.getCoefficients()
    responseGridStr = responseGrid.serialize()

    return eival, eivec, l2Error, integral, integralError, shadow1DEvaluations, \
           [bounds[0], bounds[1]], numGridPoints, responseGridStr, responseCoefficients


#---------------------Constantines AS framework----------------------------
# X        the evaluation points
# f        the objective function evaluated at X
# df       the objective functions gradient evaluated at X
# sstype   gradient based ('AS'), linear fit ('OLS') or quadratic fit ('QPHD')
# nboot    number of bootstrappings
#-------------------------------------------------------------------------- 
def ConstantineAS(X=None, f=None, df=None, responseDegree=2, sstype='AS', nboot=0, numShadow1DPoints=0, validationPoints=[], validationValues=[]):
    print(len(f))
    ss = ac.subspaces.Subspaces()
     #----------- linear fit ----------
    if sstype == 'OLS':
        ss.compute(X=X, f=f, nboot=nboot, sstype='OLS')
        eival = ss.eigenvals
        eivec = ss.eigenvecs
    #----------- quadratic fit -----------
    elif sstype == 'QPHD':
        ss.compute(X=X, f=f, nboot=nboot, sstype='QPHD')
        eival = ss.eigenvals
        eivec = ss.eigenvecs
    # ---------- exact gradient ----------
    elif sstype == 'AS':
        ss.compute(df=df, nboot=nboot, sstype='AS')
        eival = ss.eigenvals
        eivec = ss.eigenvecs
        
    print(eival)
    print("first eigenvector: {}".format(ss.eigenvecs[:, 0]))
    # quadratic polynomial approximation of maximum degree responseDegree
    print("response degree: {}".format(responseDegree))
    RS = ac.utils.response_surfaces.PolynomialApproximation(responseDegree)
    # RS = ac.utils.response_surfaces.RadialBasisApproximation(responseDegree)
    n = 1  # active subspace identifier
    ss.partition(n)
    y = X.dot(ss.W1)
    RS.train(y, f)
    
    if args.responseType == 'regular':
        # calculate l2 error in W1T * [-1,1]. 
        validationPoints = uniformX(numErrorPoints, objFunc.getDim())  
        validationValues = objFunc.eval(validationPoints, -1, 1)
    validationEval = RS.predict(np.dot(validationPoints, ss.W1))[0]
    l2Error = np.linalg.norm((validationEval - validationValues) / len(validationValues))
    print("interpol error {}".format(l2Error))
            
#     plt.scatter(np.dot(validationPoints, ss.W1), validationValues, label='f')
#     plt.scatter(np.dot(validationPoints, ss.W1), validationEval, label='approx')
#     plt.legend()
#     plt.show()
    
    # integral
    lb, ub = objFunc.getDomain()
    vol = np.prod(ub - lb)
    avdom = ac.domains.BoundedActiveVariableDomain(ss)
    avmap = ac.domains.BoundedActiveVariableMap(avdom)  
    integral = ac.integrals.av_integrate(lambda x: RS.predict(x)[0], avmap, 1000) * vol  
#     print 'Constantine Integral: {:.2f}'.format(int_I)
    integralError = abs(integral - objFunc.getIntegral())
    print("integral error {}".format(integralError))
    
    bounds = avdom.vertY[ :, 0]
    shadow1DEvaluations = []
    if numShadow1DPoints > 0:
        X1 = np.ndarray((numShadow1DPoints, 1))
        X1[:, 0] = np.linspace(bounds[0], bounds[1], numShadow1DPoints)
        shadow1DEvaluations = RS.predict(X1)[0]
    
    # plot interpolant of 2D function
#     X, Y = np.meshgrid(np.linspace(-1, 1, 50), np.linspace(-1, 1, 50))
#     Xunit, Yunit = np.meshgrid(np.linspace(0, 1, 50), np.linspace(0, 1, 50))
#     I = np.zeros(np.shape(X))
#     F = np.ndarray(np.shape(X))
#     wrapf = objFuncSGpp(objFunc)
#     for i in range(len(X)):
#         for j in range(len(X[0])):
#             p = np.ndarray((1, 2))
#             p[0, 0] = X[i, j]
#             p[0, 1] = Y[i, j]
#             I[i, j] = RS.predict(p.dot(ss.W1))[0]
#             F[i, j] = wrapf.eval([Xunit[i, j], Yunit[i, j]])
#     fig = plt.figure(); ax = fig.gca(projection='3d')
#     ax.plot_surface(Xunit, Yunit, I, cmap=cm.viridis, linewidth=0, antialiased=False)
#     ax.plot_wireframe(Xunit, Yunit, F, rstride=10, cstride=10, color='r')
#     # ax.scatter((errorPoints[:, 0] + 1) / 2.0, (errorPoints[:, 1] + 1) / 2.0, errorEval, c='b')
#     plt.show()
        
    # 1 and 2 dim shadow plots
    ss.partition(1)
    y = np.dot(X, ss.W1)
    ac.utils.plotters.sufficient_summary(y, f[:, 0])
    plt.show()
        
    print("Control:")
    print("num data points = {}".format(len(f)))
        
    return eival, eivec, l2Error, integral, integralError, shadow1DEvaluations, bounds


def Halton(objFunc, numSamples):
    print(numSamples)
    dim = objFunc.getDim()
    haltonPoints = quasiMCIntegral.halton_sequence (0, numSamples - 1, dim)
    integral = 0
    for n in range(numSamples):
        integral += objFunc.eval(haltonPoints[:, n])
    lb, ub = objFunc.getDomain()
    vol = np.prod(ub - lb)
    integral = integral[0][0] / numSamples * vol
    integralError = abs(integral - objFunc.getIntegral())
    print("quasiMC integral: {}".format(integral))
    print("quasiMC integral error: {}".format(integralError))
    return integral, integralError


#------------------------------------ main ---------------------------------------
if __name__ == "__main__":
    # parse the input arguments
    parser = ArgumentParser(description='Get a program and run it with input', version='%(prog)s 1.0')
    parser.add_argument('--model', default='sin5Dexp0.1', type=str, help="define which test case should be executed")
    parser.add_argument('--method', default='QPHD', type=str, help="asSGpp, SGpp or one of the three Constantine (AS,OLS,QPHD)")
    parser.add_argument('--numThreads', default=4, type=int, help="number of threads for omp parallelization")
    parser.add_argument('--minPoints', default=10, type=int, help="minimum number of points used")
    parser.add_argument('--maxPoints', default=100, type=int, help="maximum number of points used")
    parser.add_argument('--numSteps', default=5, type=int, help="number of steps in the [minPoints maxPoints] range")
    parser.add_argument('--saveFlag', default=1, type=bool, help="save results")
    parser.add_argument("--numShadow1DPoints", default=100, type=int, help="number of evaluations of the underlying 1D interpolant which can later be used for shadow plots")
    parser.add_argument('--numRefine', default=10, type=int, help="max number of grid points added in refinement steps for sparse grids")
    parser.add_argument('--initialLevel', default=2, type=int, help="initial regular level for adaptive sparse grids")
    parser.add_argument('--doResponse', default=1, type=int, help="do (not) create response surface")
    parser.add_argument('--doIntegral', default=1, type=int, help="do (not) calcualte integral")
    # only relevant for asSGpp and SGpp
    parser.add_argument('--gridType', default='nakbsplinemodified', type=str, help="SGpp grid type")
    parser.add_argument('--degree', default=3, type=int, help="B-spline degree / degree of Constantines resposne surface")
    parser.add_argument('--responseType', default='adaptive', type=str, help="method for response surface creation (regular,adaptive (and detection for asSGpp) ")
    # only relevant for asSGpp
    parser.add_argument('--asmType', default='adaptive', type=str, help="method for ASM creation (regular adaptive)")
    parser.add_argument('--integralType', default='Spline', type=str, help="method for integral calculation (MC, Cont)")
    parser.add_argument('--appSplineLevel', default=5, type=int, help="level used for integralType appSpline")
    parser.add_argument('--appSplineDegree', default=3, type=int, help="degree used for integralType appSpline")
    # only relevant for data
    parser.add_argument('--minDataPoints', default=10000, type=int, help="minimum number of points used in artificial data scenarios")
    parser.add_argument('--maxDataPoints', default=100000, type=int, help="maximum number of points used in artificial data scenarios")
    parser.add_argument('--numDataSteps', default=1, type=int, help="number of steps in [mindataPoints, maxDataPoints] range")
    args = parser.parse_args()
    pysgpp.omp_set_num_threads(args.numThreads)

    objFunc = asFunctions.getFunction(args.model)
    numDim = objFunc.getDim()
    sampleRange = np.unique(np.logspace(np.log10(args.minPoints), np.log10(args.maxPoints), num=args.numSteps))
    sampleRange = [int(s) for s in sampleRange]
    if args.responseType in ['data', 'dataR', 'datadriven', 'datadrivenR']:
        dataRange = np.unique(np.logspace(np.log10(args.minDataPoints), np.log10(args.maxDataPoints), num=args.numDataSteps))
        dataRange = [int(s) for s in dataRange]
    else:
        dataRange = [0]
    
    eival = np.zeros(shape=(numDim , len(sampleRange), len(dataRange)))
    eivec = np.zeros(shape=(numDim, numDim, len(sampleRange), len(dataRange)))
    durations = np.zeros(shape=(len(sampleRange), len(dataRange)))
    l2Errors = np.zeros(shape=(len(sampleRange), len(dataRange)))
    integrals = np.zeros(shape=(len(sampleRange), len(dataRange)))
    integralErrors = np.zeros(shape=(len(sampleRange), len(dataRange)))
    numGridPointsArray = np.zeros(shape=(len(sampleRange), len(dataRange)))
    shadow1DEvaluationsArray = np.zeros(shape=(args.numShadow1DPoints, len(sampleRange), len(dataRange)))
    boundsArray = np.zeros(shape=(2, len(sampleRange), len(dataRange)))
    responseGridStrDict = {}
    responseCoefficientsDict = {}
    
    if args.saveFlag == 1:
        resultsPath = "/home/rehmemk/git/SGpp/activeSubSpaces/results"
        resultsPath = os.path.join(resultsPath, objFunc.getName())
        if args.method in ['OLS', 'QPHD', 'AS']:
            folder = args.method + '_' + str(args.degree) + '_' + str(args.maxPoints) + '_' + args.responseType
        elif args.method == 'Halton':
            folder = args.method + '_' + str(args.maxPoints) 
        elif args.method == 'asSGpp': 
            folder = args.method + '_' + args.gridType + '_' + str(args.degree) + '_' + str(args.maxPoints) + '_' + args.responseType + '_' + args.asmType + '_' + args.integralType
        elif args.method == 'SGpp': 
            folder = args.method + '_' + args.gridType + '_' + str(args.degree) + '_' + str(args.maxPoints) + '_' + args.responseType
        path = os.path.join(resultsPath, folder)    
    else:
        path = None 
    
    numHistogramMCPoints = 100000
    numErrorPoints = 10000
    
    # .... ..... .... Constantines Code .... .... ....
    if args.method in ['AS', 'OLS', 'QPHD']:
        nboot = 100
        for i, numSamples in enumerate(sampleRange):
            for j, numData in enumerate(dataRange):
                start = time.time()
                df = 0; vP = 0; vV = 0  # dummy
                if args.responseType == 'data':
                    trainingPoints, trainingValues, validationPoints, validationValues = getData(path, numData, objFuncSGpp(objFunc), args.method, args.model)
                    trainingPoints.transpose();  validationPoints.transpose()
                    f = np.ndarray(shape=(trainingValues.getSize(), 1))
                    vV = np.ndarray(shape=(validationValues.getSize(), 1))
                    x = np.ndarray(shape=(trainingPoints.getNrows(), trainingPoints.getNcols()))
                    vP = np.ndarray(shape=(validationPoints.getNrows(), validationPoints.getNcols()))
                    for k in range(trainingValues.getSize()):
                        f[k] = trainingValues[k]
                        for l in range(objFunc.getDim()):
                            x[k, l] = trainingPoints.get(k, l)
                    for k in range(validationValues.getSize()):
                        vV[k] = validationValues[k]
                        for l in range(objFunc.getDim()):
                            vP[k, l] = trainingPoints.get(k, l)
                elif args.responseType == 'regular':
                    if args.model == 'borehole':
                        x = boreholeX(numSamples)
                    else:
                        x = uniformX(numSamples, numDim)
                    f = objFunc.eval(x, -1, 1)
                    if args.method == 'AS':
                        df = objFunc.eval_grad(x, -1, 1)
                else:
                    print('response type not supported')
                    
                e, v, l2Error, integral, integralError, shadow1DEvaluations, bounds = ConstantineAS(X=x, f=f, df=df, responseDegree=args.degree,
                                                                        sstype=args.method, nboot=nboot,
                                                                        numShadow1DPoints=args.numShadow1DPoints, validationPoints=vP, validationValues=vV)
                
                durations[i, j] = time.time() - start; l2Errors[i, j] = l2Error;
                integrals[i, j] = integral; integralErrors[i, j] = integralError
                shadow1DEvaluationsArray[:, i, j] = shadow1DEvaluations[:, 0]
                numGridPointsArray[i, j] = numSamples
                boundsArray[:, i, j] = bounds
                eival[:, i, j] = e[:, 0]; eivec[:, :, i, j] = v
            
    # .... .... .... Quasi Monte Carlo Integral with Halton Sequence  .... .... ....
    elif args.method == 'Halton':
        for i, numSamples in enumerate(sampleRange):
            for j, numData in enumerate(dataRange):
                start = time.time()
                integral, integralError = Halton(objFunc, numSamples)
                durations[i, j] = time.time() - start; 
                integrals[i, j] = integral; 
                integralErrors[i, j] = integralError
                numGridPointsArray[i, j] = numSamples
            
    # .... .... .... active subspace SG++ .... .... .... 
    elif args.method == 'asSGpp':
        initialLevel = 1
        numRefine = 3
        for i, numSamples in enumerate(sampleRange):
            for j, numData in enumerate(dataRange):
                start = time.time()
                numResponse = numSamples
                numASM = numSamples
                
                e, v, l2Error, integral, integralError, shadow1DEvaluations, \
                bounds, numGridPoints, responseGridStr, responseCoefficients = \
                SGppAS(objFunc, args.gridType, args.degree, numASM, numResponse, args.model,
                       args.asmType, args.responseType, args.integralType,
                       numErrorPoints=numErrorPoints, savePath=path,
                       numHistogramMCPoints=numHistogramMCPoints,
                       approxLevel=args.appSplineLevel, approxDegree=args.appSplineDegree,
                        numShadow1DPoints=args.numShadow1DPoints, numDataPoints=numData,
                        numRefine=args.numRefine, initialLevel=args.initialLevel,
                        doResponse=args.doResponse, doIntegral=args.doIntegral)
                
                durations[i, j] = time.time() - start; l2Errors[i, j] = l2Error;
                integrals[i, j] = integral; integralErrors[i, j] = integralError
                shadow1DEvaluationsArray[:, i, j] = shadow1DEvaluations
                numGridPointsArray[i, j] = numGridPoints
                boundsArray[:, i, j] = bounds
                eival[:, i, j] = e[:]
                eivec[:, :, i, j] = v
                responseGridStrDict["{} {}".format(i, j)] = responseGridStr
                responseCoefficientsDict["{} {}".format(i, j)] = dataVectorToPy(responseCoefficients)
            
    # .... .... .... .... SG++ .... .... .... ....
    elif args.method == 'SGpp':
        initialLevel = 1
        numRefine = 3
        for i, numSamples in enumerate(sampleRange):
            for j, numData in enumerate(dataRange):
                start = time.time()
                
                l2Error, integral, integralError, numGridPoints = \
                SGpp(objFunc, args.gridType, args.degree, numSamples,
                     args.model, args.responseType, numErrorPoints=numErrorPoints,
                     savePath=path, numDataPoints=numData,
                        numRefine=args.numRefine, initialLevel=args.initialLevel)
                
                durations[i, j] = time.time() - start; l2Errors[i, j] = l2Error;
                numGridPointsArray[i, j] = numGridPoints
                integrals[i, j] = integral; integralErrors[i, j] = integralError
    #------------------------------------ save Data ---------------------------------------
    if args.saveFlag == True:
        print("saving Data to {}".format(path))
        if not os.path.exists(path):
            os.makedirs(path)
        # encapuslate all results and the input in 'summary' dictionary and save it
        summary = {'eigenvalues':eival, 'eigenvectors':eivec, 'sampleRange':sampleRange,
                'durations':durations, 'dim': numDim, 'l2Errors': l2Errors,
                'integrals':integrals, 'integralErrors': integralErrors,
                'model':args.model, 'method':args.method, 'minPoints':args.minPoints,
                'maxPoints':args.maxPoints, 'numSteps':args.numSteps, 'gridType':args.gridType,
                'degree':args.degree, 'responseType':args.responseType, 'asmType':args.asmType,
                'integralType': args.integralType, 'numHistogramMCPoints':numHistogramMCPoints,
                'appSplineLevel': args.appSplineLevel, 'appSplineDegree': args.appSplineDegree,
                'numShadow1DPoints':args.numShadow1DPoints, 'shadow1DEvaluationsArray':shadow1DEvaluationsArray,
                'boundsArray':boundsArray, 'numGridPointsArray':numGridPointsArray, 'dataRange':dataRange,
                'responseGridStrsDict':responseGridStrDict, 'responseCoefficientsDict': responseCoefficientsDict,
                'numRefine':args.numRefine, 'initialLevel':args.initialLevel}
        with open(os.path.join(path, 'summary.pkl'), 'wb') as fp:
            pickle.dump(summary, fp)
