/*======================================================================

  This file is part of the elastix software.

  Copyright (c) University Medical Center Utrecht. All rights reserved.
  See src/CopyrightElastix.txt or http://elastix.isi.uu.nl/legal.php for
  details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE. See the above copyright notices for more information.

======================================================================*/

// OpenCL implementation of itk::IdentityTransform

//------------------------------------------------------------------------------
#ifdef DIM_1
float identity_transform_point_1d(const float point)
{
  return point;
}

#endif // DIM_1

//------------------------------------------------------------------------------
#ifdef DIM_2
float2 identity_transform_point_2d(const float2 point)
{
  return point;
}

#endif // DIM_2

//------------------------------------------------------------------------------
#ifdef DIM_3
float3 identity_transform_point_3d(const float3 point)
{
  return point;
}

#endif // DIM_3