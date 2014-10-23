/*======================================================================

  This file is part of the elastix software.

  Copyright (c) University Medical Center Utrecht. All rights reserved.
  See src/CopyrightElastix.txt or http://elastix.isi.uu.nl/legal.php for
  details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE. See the above copyright notices for more information.

======================================================================*/
#ifndef __itkRecursiveBSplineTransform_hxx
#define __itkRecursiveBSplineTransform_hxx

#include "itkRecursiveBSplineTransform.h"


namespace itk
{

/**
 * ********************* Constructor ****************************
 */

template <typename TScalar, unsigned int NDimensions, unsigned int VSplineOrder>
RecursiveBSplineTransform< TScalar, NDimensions, VSplineOrder >
::RecursiveBSplineTransform() : Superclass()
{
  this->m_RecursiveBSplineWeightFunction = RecursiveBSplineWeightFunctionType::New();
} // end Constructor()


/**
 * ********************* TransformPoint ****************************
 */

// MS: this is slightly different from AdvancedBSplineDeformableTransform
// should we update that one and delete this one?
template <typename TScalar, unsigned int NDimensions, unsigned int VSplineOrder>
typename RecursiveBSplineTransform<TScalar, NDimensions, VSplineOrder>
::OutputPointType
RecursiveBSplineTransform<TScalar, NDimensions, VSplineOrder>
::TransformPoint( const InputPointType & point ) const
{
  /** Allocate memory on the stack: */
  const unsigned int numberOfWeights = RecursiveBSplineWeightFunctionType::NumberOfWeights;
  const unsigned int numberOfIndices = RecursiveBSplineWeightFunctionType::NumberOfIndices;
  typename WeightsType::ValueType weightsArray[ numberOfWeights ];
  typename ParameterIndexArrayType::ValueType indicesArray[ numberOfIndices ];
  WeightsType             weights( weightsArray, numberOfWeights, false );
  ParameterIndexArrayType indices( indicesArray, numberOfIndices, false );

  OutputPointType         outputPoint;
  bool                    inside;

  this->TransformPoint( point, outputPoint, weights, indices, inside );

  return outputPoint;
} // end TransformPoint()


/**
 * ********************* TransformPoint ****************************
 */

template< typename TScalar, unsigned int NDimensions, unsigned int VSplineOrder >
void
RecursiveBSplineTransform<TScalar, NDimensions, VSplineOrder>
::TransformPoint(
  const InputPointType & point,
  OutputPointType & outputPoint,
  WeightsType & weights,
  ParameterIndexArrayType & indices,
  bool & inside ) const
{
  inside = true;
  InputPointType transformedPoint = point;

  /** Check if the coefficient image has been set. */
  if( !this->m_CoefficientImages[ 0 ] )
  {
    itkWarningMacro( << "B-spline coefficients have not been set" );
    for( unsigned int j = 0; j < SpaceDimension; j++ )
    {
      outputPoint[ j ] = transformedPoint[ j ];
    }
    return;
  }

  /***/
  ContinuousIndexType cindex;
  this->TransformPointToContinuousGridIndex( point, cindex );

  // NOTE: if the support region does not lie totally within the grid
  // we assume zero displacement and return the input point
  inside = this->InsideValidRegion( cindex );
  if( !inside )
  {
    outputPoint = transformedPoint;
    return;
  }

  // Compute interpolation weighs and store them in weights
  // MS: compare with AdvancedBSplineDeformableTransform
  IndexType supportIndex;
  this->m_RecursiveBSplineWeightFunction->Evaluate( cindex, weights, supportIndex );

  // Allocation of memory
  long evaluateIndexData[ ( SplineOrder + 1 ) * SpaceDimension ];
  long stepsData[ ( SplineOrder + 1 ) * SpaceDimension ];
  vnl_matrix_ref<long> evaluateIndex( SpaceDimension, SplineOrder + 1, evaluateIndexData );
  double * weightsPointer = &(weights[0]);
  long * steps = &(stepsData[0]);

  for( unsigned int ii = 0; ii < SpaceDimension; ++ii )
  {
    for( unsigned int jj = 0; jj <= SplineOrder; ++jj )
    {
      evaluateIndex[ ii ][ jj ] = supportIndex[ ii ] + jj;
    }
  }

  IndexType offsetTable;
  for( unsigned int n = 0; n < SpaceDimension; ++n )
  {
    offsetTable[ n ] = this->m_CoefficientImages[ 0 ]->GetOffsetTable()[ n ];
    for( unsigned int k = 0; k <= SplineOrder; ++k )
    {
      steps[ ( SplineOrder + 1 ) * n + k ] = evaluateIndex[ n ][ k ] * offsetTable[ n ];
    }
  }

  // Call recursive interpolate function
  // MS: I think that this can be made more efficient. Now the product of beta's are recomputed
  // for each dimension, while these are the same. Only mu = basepointer changes
#if 1
  outputPoint.Fill( NumericTraits<TScalar>::Zero );
  for( unsigned int j = 0; j < SpaceDimension; ++j )
  {
    const TScalar *basePointer = this->m_CoefficientImages[ j ]->GetBufferPointer();
    unsigned int c = 0;
    outputPoint[ j ] = RecursiveBSplineTransformImplementation< SpaceDimension, SplineOrder, TScalar >
      ::InterpolateTransformPoint( basePointer,
      steps,
      weightsPointer,
      basePointer,
      indices,
      c );

    // The output point is the start point + displacement.
    outputPoint[ j ] += transformedPoint[ j ];
  } // end for
#else
  outputPoint.Fill( NumericTraits<TScalar>::Zero );
  typedef FixedArray< TScalar *, SpaceDimension > ScalarPointerVectorType;
  ScalarPointerVectorType basePointers;
  for( unsigned int j = 0; j < SpaceDimension; ++j )
  {
    basePointers[ j ] = this->m_CoefficientImages[ j ]->GetBufferPointer();
  }
  //const TScalar *basePointer = this->m_CoefficientImages[ j ]->GetBufferPointer();
  unsigned int c = 0;
  outputPoint = RecursiveBSplineTransformImplementation< SpaceDimension, SplineOrder, TScalar >
    ::InterpolateTransformPoint2( basePointers, steps,
      weightsPointer, basePointers, indices, c );

  // The output point is the start point + displacement.
  //outputPoint += transformedPoint;
#endif
} // end TransformPoint()


/**
 * ********************* GetJacobian ****************************
 */

template< class TScalar, unsigned int NDimensions, unsigned int VSplineOrder >
void
RecursiveBSplineTransform< TScalar, NDimensions, VSplineOrder >
::GetJacobian( const InputPointType & ipp, JacobianType & jacobian,
  NonZeroJacobianIndicesType & nonZeroJacobianIndices ) const
{
  //Superclass::GetJacobian( ipp, j, nzji );

  /** Convert the physical point to a continuous index, which
   * is needed for the 'Evaluate()' functions below.
   */
  ContinuousIndexType cindex;
  this->TransformPointToContinuousGridIndex( ipp, cindex );

  /** Initialize. */
  const NumberOfParametersType nnzji = this->GetNumberOfNonZeroJacobianIndices();
  if( ( jacobian.cols() != nnzji ) || ( jacobian.rows() != SpaceDimension ) )
  {
    jacobian.SetSize( SpaceDimension, nnzji );
    jacobian.Fill( 0.0 );
  }

  /** NOTE: if the support region does not lie totally within the grid
   * we assume zero displacement and zero Jacobian.
   */
  if( !this->InsideValidRegion( cindex ) )
  {
    nonZeroJacobianIndices.resize( this->GetNumberOfNonZeroJacobianIndices() );
    for( NumberOfParametersType i = 0; i < this->GetNumberOfNonZeroJacobianIndices(); ++i )
    {
      nonZeroJacobianIndices[ i ] = i;
    }
    return;
  }

  /** Compute the number of affected B-spline parameters.
   * Allocate memory on the stack.
   */
  //const unsigned long numberOfWeights = WeightsFunctionType::NumberOfWeights;
  //typename WeightsType::ValueType weightsArray[ numberOfWeights ];
  //WeightsType weights( weightsArray, numberOfWeights, false );

  /** Compute the interpolation weights. 
   * In contrast to the normal B-spline weights function, the recursive version
   * returns the individual weights instead of the multiplied ones.
   */
  IndexType supportIndex;
  //this->m_WeightsFunction->ComputeStartIndex( cindex, supportIndex );
  //this->m_WeightsFunction->Evaluate( cindex, supportIndex, weights );

  const unsigned int numberOfWeights = RecursiveBSplineWeightFunctionType::NumberOfWeights;
  const unsigned int numberOfIndices = RecursiveBSplineWeightFunctionType::NumberOfIndices;
  typename WeightsType::ValueType weightsArray1D[ numberOfWeights ];
  typename ParameterIndexArrayType::ValueType indicesArray[ numberOfIndices ];
  WeightsType             weights1D( weightsArray1D, numberOfWeights, false );
  ParameterIndexArrayType indices( indicesArray, numberOfIndices, false );
  this->m_RecursiveBSplineWeightFunction->Evaluate( cindex, weights1D, supportIndex );

  // Allocation of memory, just copied from the transform point above, need to check
  long evaluateIndexData[ ( SplineOrder + 1 ) * SpaceDimension ];
  long stepsData[ ( SplineOrder + 1 ) * SpaceDimension ];
  vnl_matrix_ref<long> evaluateIndex( SpaceDimension, SplineOrder + 1, evaluateIndexData );
  double * weights1DPointer = &(weights1D[0]);
  long * steps = &(stepsData[0]);

  for( unsigned int ii = 0; ii < SpaceDimension; ++ii )
  {
    for( unsigned int jj = 0; jj <= SplineOrder; ++jj )
    {
      evaluateIndex[ ii ][ jj ] = supportIndex[ ii ] + jj;
    }
  }

  IndexType offsetTable;
  for( unsigned int n = 0; n < SpaceDimension; ++n )
  {
    offsetTable[ n ] = this->m_CoefficientImages[ 0 ]->GetOffsetTable()[ n ];
    for( unsigned int k = 0; k <= SplineOrder; ++k )
    {
      steps[ ( SplineOrder + 1 ) * n + k ] = evaluateIndex[ n ][ k ] * offsetTable[ n ];
    }
  }

  // Call the recursive function, this is where the magic happens
  //outputPoint.Fill( NumericTraits<TScalar>::Zero );
  unsigned int ii = 0;
  typename WeightsType::ValueType weightsArray[ numberOfIndices ];
  for( unsigned int j = 0; j < SpaceDimension; ++j )
  {
    const TScalar *basePointer = this->m_CoefficientImages[ j ]->GetBufferPointer(); // not needed
    unsigned int c = 0;
    //weightsArray[ ii ] = 
    RecursiveBSplineTransformImplementation< SpaceDimension, SplineOrder, TScalar >
      ::InterpolateGetJacobian( basePointer, steps, weights1DPointer, basePointer, indices, c );
    ii += 64;
  } // end for

  /** Put at the right positions. */
  ParametersValueType * jacobianPointer = jacobian.data_block();
  for( unsigned int d = 0; d < SpaceDimension; ++d )
  {
    unsigned long offset = d * SpaceDimension * numberOfWeights + d * numberOfWeights;
    std::copy( weightsArray, weightsArray + numberOfWeights, jacobianPointer + offset );
  }


  /** Setup support region *
  RegionType supportRegion;
  supportRegion.SetSize( this->m_SupportSize );
  supportRegion.SetIndex( supportIndex );

  /** Put at the right positions. *
  ParametersValueType * jacobianPointer = jacobian.data_block();
  for( unsigned int d = 0; d < SpaceDimension; ++d )
  {
    unsigned long offset = d * SpaceDimension * numberOfWeights + d * numberOfWeights;
    std::copy( weightsArray, weightsArray + numberOfWeights, jacobianPointer + offset );
  }

  /** Compute the nonzero Jacobian indices.
   * Takes a significant portion of the computation time of this function.
   */
  //this->ComputeNonZeroJacobianIndices( nonZeroJacobianIndices, supportRegion );

} // end GetJacobian()


/**
 * ********************* GetSpatialJacobian ****************************
 */

template< class TScalar, unsigned int NDimensions, unsigned int VSplineOrder >
void
RecursiveBSplineTransform< TScalar, NDimensions, VSplineOrder >
::GetSpatialJacobian(
  const InputPointType & ipp,
  SpatialJacobianType & sj ) const
{
  /** Convert the physical point to a continuous index, which
   * is needed for the 'Evaluate()' functions below.
   */
  ContinuousIndexType cindex;
  this->TransformPointToContinuousGridIndex( ipp, cindex );

  // NOTE: if the support region does not lie totally within the grid
  // we assume zero displacement and identity spatial Jacobian
  if( !this->InsideValidRegion( cindex ) )
  {
    sj.SetIdentity();
    return;
  }

  /** Compute the number of affected B-spline parameters. */
  /** Allocate memory on the stack: */
  const unsigned long numberOfWeights = WeightsFunctionType::NumberOfWeights;
  typename WeightsType::ValueType weightsArray[ numberOfWeights ];
  WeightsType weights( weightsArray, numberOfWeights, false );

  typename WeightsType::ValueType derivativeWeightsArray[ numberOfWeights ];
  WeightsType derivativeWeights( derivativeWeightsArray, numberOfWeights, false );

  IndexType supportIndex;
  this->m_DerivativeWeightsFunctions[ 0 ]->ComputeStartIndex(
    cindex, supportIndex );
  RegionType supportRegion;
  supportRegion.SetSize( this->m_SupportSize );
  supportRegion.SetIndex( supportIndex );

  //    /** Compute the spatial Jacobian sj:
  //   *    dT_{dim} / dx_i = delta_{dim,i} + \sum coefs_{dim} * weights * PointToGridIndex.
  //   */
  //    typedef ImageScanlineConstIterator< ImageType > IteratorType;
  //    sj.Fill( 0.0 );
  //    for( unsigned int i = 0; i < SpaceDimension; ++i )
  //    {
  //        /** Compute the derivative weights. */
  //        this->m_DerivativeWeightsFunctions[ i ]->Evaluate( cindex, supportIndex, weights );

  //        /** Compute the spatial Jacobian sj:
  //     *    dT_{dim} / dx_i = \sum coefs_{dim} * weights.
  //     */
  //        for( unsigned int dim = 0; dim < SpaceDimension; ++dim )
  //        {
  //            /** Create an iterator over the correct part of the coefficient
  //       * image. Create an iterator over the weights vector.
  //       */
  //            IteratorType itCoef( this->m_CoefficientImages[ dim ], supportRegion );
  //            typename WeightsType::const_iterator itWeights = weights.begin();

  //            /** Compute the sum for this dimension. */
  //            double sum = 0.0;
  //            while( !itCoef.IsAtEnd() )
  //            {
  //                while( !itCoef.IsAtEndOfLine() )
  //                {
  //                    sum += itCoef.Value() * ( *itWeights );
  //                    ++itWeights;
  //                    ++itCoef;
  //                }
  //                itCoef.NextLine();
  //            }

  //            /** Update the spatial Jacobian sj. */
  //            sj( dim, i ) += sum;

  //        } // end for dim
  //    }   // end for i

  // Compute interpolation weighs and store them in weights
  this->m_RecursiveBSplineWeightFunction->Evaluate( cindex, weights, supportIndex );
  this->m_RecursiveBSplineWeightFunction->EvaluateDerivative( cindex, derivativeWeights, supportIndex );

  //Allocation of memory
  // MS: The following is a copy from TransformPoint and candidate for refactoring
  long evaluateIndexData[ ( SplineOrder + 1 ) * SpaceDimension ];
  long stepsData[ ( SplineOrder + 1 ) * SpaceDimension ];
  vnl_matrix_ref<long> evaluateIndex( SpaceDimension, SplineOrder + 1, evaluateIndexData );
  double * weightsPointer = &(weights[0]);
  double * derivativeWeightsPointer = &(derivativeWeights[0]);
  long * steps = &(stepsData[0]);

  for( unsigned int ii = 0; ii < SpaceDimension; ++ii )
  {
    for( unsigned int jj = 0; jj <= SplineOrder; ++jj )
    {
      evaluateIndex[ ii ][ jj ] = supportIndex[ ii ] + jj;
    }
  }

  IndexType offsetTable;
  for( unsigned int n = 0; n < SpaceDimension; ++n )
  {
    offsetTable[ n ] = this->m_CoefficientImages[ 0 ]->GetOffsetTable()[ n ];
    for( unsigned int k = 0; k <= SplineOrder; ++k )
    {
      steps[ ( SplineOrder + 1 ) * n + k ] = evaluateIndex[ n ][ k ] * offsetTable[ n ];
    }
  }

  // Call recursive interpolate function
  for( unsigned int j = 0; j < SpaceDimension; ++j )
  {
    TScalar derivativeValue[ SpaceDimension + 1 ];
    const TScalar *basePointer = this->m_CoefficientImages[ j ]->GetBufferPointer();
    RecursiveBSplineTransformImplementation< SpaceDimension, SplineOrder, TScalar >
      ::InterpolateSpatialJacobian( derivativeValue,
      basePointer,
      steps,
      weightsPointer,
      derivativeWeightsPointer );

    for( unsigned int dim = 0; dim < SpaceDimension; ++dim )
    {
      sj( dim, j ) = derivativeValue[ dim + 1 ]; //First element of derivativeValue is the value, not the derivative.
    }

    sj( j, j ) += 1.0;
  } // end for

  /** Take into account grid spacing and direction cosines. */
  sj = sj * this->m_PointToIndexMatrix2;

} // end GetSpatialJacobian()


}// namespace


#endif
