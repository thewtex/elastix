/*=========================================================================
 *
 *  Copyright UMC Utrecht and contributors
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/
/*=========================================================================

  Program:   Insight Segmentation & Registration Toolkit
  Module:    $RCSfile: itkAdvancedVersorRigid3DTransform.txx,v $
  Date:      $Date: 2006-03-19 04:36:59 $
  Version:   $Revision: 1.32 $

  Copyright (c) Insight Software Consortium. All rights reserved.
  See ITKCopyright.txt or http://www.itk.org/HTML/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef itkAdvancedVersorRigid3DTransform_hxx
#define itkAdvancedVersorRigid3DTransform_hxx

#include "itkAdvancedVersorRigid3DTransform.h"

namespace itk
{

// Constructor with default arguments
template <typename TScalarType>
AdvancedVersorRigid3DTransform<TScalarType>::AdvancedVersorRigid3DTransform()
  : Superclass(ParametersDimension)
{}

// Constructor with arguments
template <typename TScalarType>
AdvancedVersorRigid3DTransform<TScalarType>::AdvancedVersorRigid3DTransform(unsigned int paramDim)
  : Superclass(paramDim)
{}

// Constructor with arguments
template <typename TScalarType>
AdvancedVersorRigid3DTransform<TScalarType>::AdvancedVersorRigid3DTransform(const MatrixType &       matrix,
                                                                            const OutputVectorType & offset)
  : Superclass(matrix, offset)
{}

// Set Parameters
template <typename TScalarType>
void
AdvancedVersorRigid3DTransform<TScalarType>::SetParameters(const ParametersType & parameters)
{

  itkDebugMacro("Setting parameters " << parameters);

  // Transfer the versor part

  AxisType axis;

  double norm = parameters[0] * parameters[0];
  axis[0] = parameters[0];
  norm += parameters[1] * parameters[1];
  axis[1] = parameters[1];
  norm += parameters[2] * parameters[2];
  axis[2] = parameters[2];
  if (norm > 0)
  {
    norm = std::sqrt(norm);
  }

  double epsilon = 1e-10;
  if (norm >= 1.0 - epsilon)
  {
    axis = axis / (norm + epsilon * norm);
  }
  VersorType newVersor;
  newVersor.Set(axis);
  this->SetVarVersor(newVersor);
  this->ComputeMatrix();

  itkDebugMacro("Versor is now " << this->GetVersor());

  // Transfer the translation part
  TranslationType newTranslation;
  newTranslation[0] = parameters[3];
  newTranslation[1] = parameters[4];
  newTranslation[2] = parameters[5];
  this->SetVarTranslation(newTranslation);
  this->ComputeOffset();

  // Modified is always called since we just have a pointer to the
  // parameters and cannot know if the parameters have changed.
  this->Modified();

  itkDebugMacro("After setting parameters ");
}


//
// Get Parameters
//
// Parameters are ordered as:
//
// p[0:2] = right part of the versor (axis times std::sin(t/2))
// p[3:5} = translation components
//

template <typename TScalarType>
auto
AdvancedVersorRigid3DTransform<TScalarType>::GetParameters() const -> const ParametersType &
{
  itkDebugMacro("Getting parameters ");

  this->m_Parameters[0] = this->GetVersor().GetX();
  this->m_Parameters[1] = this->GetVersor().GetY();
  this->m_Parameters[2] = this->GetVersor().GetZ();

  // Transfer the translation
  this->m_Parameters[3] = this->GetTranslation()[0];
  this->m_Parameters[4] = this->GetTranslation()[1];
  this->m_Parameters[5] = this->GetTranslation()[2];

  itkDebugMacro("After getting parameters " << this->m_Parameters);

  return this->m_Parameters;
}

// Set parameters
template <typename TScalarType>
void
AdvancedVersorRigid3DTransform<TScalarType>::GetJacobian(const InputPointType &       p,
                                                         JacobianType &               j,
                                                         NonZeroJacobianIndicesType & nzji) const
{
  using ValueType = typename VersorType::ValueType;

  // Initialize the Jacobian. Resizing is only performed when needed.
  // Filling with zeros is needed because the lower loops only visit
  // the nonzero positions.
  j.set_size(OutputSpaceDimension, ParametersDimension);
  j.fill(0.0);

  // compute derivatives with respect to rotation
  const ValueType vx = this->GetVersor().GetX();
  const ValueType vy = this->GetVersor().GetY();
  const ValueType vz = this->GetVersor().GetZ();
  const ValueType vw = this->GetVersor().GetW();

  const double px = p[0] - this->GetCenter()[0];
  const double py = p[1] - this->GetCenter()[1];
  const double pz = p[2] - this->GetCenter()[2];

  const double vxx = vx * vx;
  const double vyy = vy * vy;
  const double vzz = vz * vz;
  const double vww = vw * vw;

  const double vxy = vx * vy;
  const double vxz = vx * vz;
  const double vxw = vx * vw;

  const double vyz = vy * vz;
  const double vyw = vy * vw;

  const double vzw = vz * vw;

  // compute Jacobian with respect to quaternion parameters
  j[0][0] = 2.0 * ((vyw + vxz) * py + (vzw - vxy) * pz) / vw;
  j[1][0] = 2.0 * ((vyw - vxz) * px - 2 * vxw * py + (vxx - vww) * pz) / vw;
  j[2][0] = 2.0 * ((vzw + vxy) * px + (vww - vxx) * py - 2 * vxw * pz) / vw;

  j[0][1] = 2.0 * (-2 * vyw * px + (vxw + vyz) * py + (vww - vyy) * pz) / vw;
  j[1][1] = 2.0 * ((vxw - vyz) * px + (vzw + vxy) * pz) / vw;
  j[2][1] = 2.0 * ((vyy - vww) * px + (vzw - vxy) * py - 2 * vyw * pz) / vw;

  j[0][2] = 2.0 * (-2 * vzw * px + (vzz - vww) * py + (vxw - vyz) * pz) / vw;
  j[1][2] = 2.0 * ((vww - vzz) * px - 2 * vzw * py + (vyw + vxz) * pz) / vw;
  j[2][2] = 2.0 * ((vxw + vyz) * px + (vyw - vxz) * py) / vw;

  j[0][3] = 1.0;
  j[1][4] = 1.0;
  j[2][5] = 1.0;

  // Copy the constant nonZeroJacobianIndices
  nzji = this->m_NonZeroJacobianIndices;
}

} // namespace itk

#endif
