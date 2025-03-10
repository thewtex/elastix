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
#ifndef elxSplineKernelTransform_hxx
#define elxSplineKernelTransform_hxx

#include "elxSplineKernelTransform.h"
#include "itkTransformixInputPointFileReader.h"
#include <vnl/vnl_math.h>
#include "itkDeref.h"
#include "itkTimeProbe.h"
#include <cstdint> // For int64_t.

namespace elastix
{

/**
 * ********************* Constructor ****************************
 */

template <typename TElastix>
SplineKernelTransform<TElastix>::SplineKernelTransform()
{
  this->SetKernelType("unknown");
} // end Constructor


/*
 * ******************* SetKernelType ***********************
 */

template <typename TElastix>
bool
SplineKernelTransform<TElastix>::SetKernelType(const std::string & kernelType)
{
  this->m_SplineKernelType = kernelType;

  /** According to VTK documentation the R2logR version is
   * appropriate for 2D and the normal for 3D
   * \todo: understand why
   */
  if constexpr (SpaceDimension == 2)
  {
    /** only one variant for 2D possible: */
    this->m_KernelTransform = TPRKernelTransformType::New();
  }
  else
  {
    /** 3D: choose between different spline types.
     * \todo: devise one for 4D
     */
    if (kernelType == "ThinPlateSpline")
    {
      this->m_KernelTransform = TPKernelTransformType::New();
    }
    //     else if ( kernelType == "ThinPlateR2LogRSpline" )
    //     {
    //       this->m_KernelTransform = TPRKernelTransformType::New();
    //     }
    else if (kernelType == "VolumeSpline")
    {
      this->m_KernelTransform = VKernelTransformType::New();
    }
    else if (kernelType == "ElasticBodySpline")
    {
      this->m_KernelTransform = EBKernelTransformType::New();
    }
    else if (kernelType == "ElasticBodyReciprocalSpline")
    {
      this->m_KernelTransform = EBRKernelTransformType::New();
    }
    else
    {
      /** unknown kernelType */
      this->m_KernelTransform = KernelTransformType::New();
      return false;
    }
  }

  this->SetCurrentTransform(this->m_KernelTransform);
  return true;

} // end SetKernelType()


/*
 * ******************* BeforeAll ***********************
 */

template <typename TElastix>
int
SplineKernelTransform<TElastix>::BeforeAll()
{
  const Configuration & configuration = itk::Deref(Superclass2::GetConfiguration());

  /** Check if -fp is given */
  /** If the optional command "-fp" is given in the command
   * line arguments, then and only then we continue.
   */
  // fp used to be ipp, added in elastix 4.5
  std::string ipp = configuration.GetCommandLineArgument("-ipp");
  std::string fp = configuration.GetCommandLineArgument("-fp");

  // Backwards compatibility stuff:
  if (!ipp.empty())
  {
    log::warn("WARNING: -ipp is deprecated, use -fp instead.");

    if (fp.empty())
    {
      fp = ipp;
    }
  }

  /** Is the fixed landmark file specified? */
  if (ipp.empty() && fp.empty())
  {
    log::error(std::ostringstream{} << "ERROR: -fp should be given for " << this->elxGetClassName()
                                    << " in order to define the fixed image (source) landmarks.");
    return 1;
  }
  else
  {
    log::info(std::ostringstream{} << "-fp       " << fp);
  }

  /** Check if -mp is given. If the optional command "-mp"
   * is given in the command line arguments, then we print it.
   */
  std::string mp = configuration.GetCommandLineArgument("-mp");

  /** Is the moving landmark file specified? */
  if (mp.empty())
  {
    log::info("-mp       unspecified, assumed equal to -fp");
  }
  else
  {
    log::info(std::ostringstream{} << "-mp       " << mp);
  }

  return 0;

} // end BeforeAll()


/*
 * ******************* BeforeRegistration ***********************
 */

template <typename TElastix>
void
SplineKernelTransform<TElastix>::BeforeRegistration()
{
  const Configuration & configuration = itk::Deref(Superclass2::GetConfiguration());

  /** Determine type of spline. */
  std::string kernelType = "ThinPlateSpline";
  configuration.ReadParameter(kernelType, "SplineKernelType", this->GetComponentLabel(), 0, -1);
  bool knownType = this->SetKernelType(kernelType);
  if (!knownType)
  {
    log::error(std::ostringstream{} << "ERROR: The kernel type " << kernelType << " is not supported.");
    itkExceptionMacro("ERROR: unable to configure " << this->GetComponentLabel());
  }

  /** Interpolating or approximating spline. */
  double splineRelaxationFactor = 0.0;
  configuration.ReadParameter(splineRelaxationFactor, "SplineRelaxationFactor", this->GetComponentLabel(), 0, -1);
  this->m_KernelTransform->SetStiffness(splineRelaxationFactor);

  /** Set the Poisson ratio; default = 0.3 = steel. */
  if (kernelType == "ElasticBodySpline" || kernelType == "ElastixBodyReciprocalSpline")
  {
    double poissonRatio = 0.3;
    configuration.ReadParameter(poissonRatio, "SplinePoissonRatio", this->GetComponentLabel(), 0, -1);
    this->m_KernelTransform->SetPoissonRatio(poissonRatio);
  }

  /** Set the matrix inversion method (one of {SVD, QR}). */
  std::string matrixInversionMethod = "SVD";
  configuration.ReadParameter(matrixInversionMethod, "TPSMatrixInversionMethod", 0, true);
  this->m_KernelTransform->SetMatrixInversionMethod(matrixInversionMethod);

  /** Load fixed image (source) landmark positions. */
  this->DetermineSourceLandmarks();

  /** Load moving image (target) landmark positions. */
  bool movingLandmarksGiven = this->DetermineTargetLandmarks();

  /** Set all parameters to identity if no moving landmarks were given. */
  if (!movingLandmarksGiven)
  {
    this->m_KernelTransform->SetIdentity();
  }

  /** Set the initial parameters in this->m_Registration. */
  this->m_Registration->GetAsITKBaseType()->SetInitialTransformParameters(this->GetParameters());

  /** \todo: builtin some multiresolution in this transform. */

} // end BeforeRegistration()


/**
 * ************************* DetermineSourceLandmarks *********************
 */

template <typename TElastix>
void
SplineKernelTransform<TElastix>::DetermineSourceLandmarks()
{
  const Configuration & configuration = itk::Deref(Superclass2::GetConfiguration());

  /** Load the fixed image landmarks. */
  log::info(std::ostringstream{} << "Loading fixed image landmarks for " << this->GetComponentLabel() << ":"
                                 << this->elxGetClassName() << ".");

  std::string fp = configuration.GetCommandLineArgument("-fp");
  if (fp.empty())
  {
    // backwards compatibility, added in elastix 4.5
    // fp used to be ipp
    fp = configuration.GetCommandLineArgument("-ipp");
  }
  PointSetPointer landmarkPointSet; // default-constructed (null)
  this->ReadLandmarkFile(fp, landmarkPointSet, true);

  /** Set the fp as source landmarks. */
  itk::TimeProbe timer;
  timer.Start();
  log::info("  Setting the fixed image landmarks (requiring large matrix inversion) ...");
  this->m_KernelTransform->SetSourceLandmarks(landmarkPointSet);
  timer.Stop();
  log::info(std::ostringstream{} << "  Setting the fixed image landmarks took: "
                                 << Conversion::SecondsToDHMS(timer.GetMean(), 6));

} // end DetermineSourceLandmarks()


/**
 * ************************* DetermineTargetLandmarks *********************
 */

template <typename TElastix>
bool
SplineKernelTransform<TElastix>::DetermineTargetLandmarks()
{
  /** The moving landmark file name. */
  std::string mp = this->GetConfiguration()->GetCommandLineArgument("-mp");
  if (mp.empty())
  {
    return false;
  }

  /** Load the moving image landmarks. */
  log::info(std::ostringstream{} << "Loading moving image landmarks for " << this->GetComponentLabel() << ":"
                                 << this->elxGetClassName() << ".");

  PointSetPointer landmarkPointSet;
  this->ReadLandmarkFile(mp, landmarkPointSet, false);

  /** Set the mp as target landmarks. */
  itk::TimeProbe timer;
  timer.Start();
  log::info("  Setting the moving image landmarks ...");
  this->m_KernelTransform->SetTargetLandmarks(landmarkPointSet);
  timer.Stop();
  log::info(std::ostringstream{} << "  Setting the moving image landmarks took: "
                                 << Conversion::SecondsToDHMS(timer.GetMean(), 6));

  return true;

} // end DetermineTargetLandmarks()


/**
 * ************************* ReadLandmarkFile *********************
 */

template <typename TElastix>
void
SplineKernelTransform<TElastix>::ReadLandmarkFile(const std::string & filename,
                                                  PointSetPointer &   landmarkPointSet,
                                                  const bool          landmarksInFixedImage)
{
  /** Typedef's. */
  using IndexType = typename FixedImageType::IndexType;
  using IndexValueType = typename IndexType::IndexValueType;

  /** Construct a landmark file reader and read the points. */
  auto landmarkReader = itk::TransformixInputPointFileReader<PointSetType>::New();
  landmarkReader->SetFileName(filename);
  try
  {
    landmarkReader->Update();
  }
  catch (const itk::ExceptionObject & err)
  {
    log::error(std::ostringstream{} << "  Error while opening landmark file.\n" << err);
    itkExceptionMacro("ERROR: unable to configure " << this->GetComponentLabel());
  }

  /** Some user-feedback. */
  if (landmarkReader->GetPointsAreIndices())
  {
    log::info("  Landmarks are specified as image indices.");
  }
  else
  {
    log::info("  Landmarks are specified in world coordinates.");
  }
  const unsigned int nrofpoints = landmarkReader->GetNumberOfPoints();
  log::info(std::ostringstream{} << "  Number of specified input points: " << nrofpoints);

  /** Get the set of input points. */
  landmarkPointSet = landmarkReader->GetOutput();

  /** Convert from index to point if necessary */
  landmarkPointSet->DisconnectPipeline();
  if (landmarkReader->GetPointsAreIndices())
  {
    /** Get handles to the fixed and moving images. */
    typename FixedImageType::Pointer  fixedImage = this->GetElastix()->GetFixedImage();
    typename MovingImageType::Pointer movingImage = this->GetElastix()->GetMovingImage();

    InputPointType landmarkPoint{};
    IndexType      landmarkIndex;
    for (unsigned int j = 0; j < nrofpoints; ++j)
    {
      /** The read point from the inputPointSet is actually an index
       * Cast to the proper type.
       */
      landmarkPointSet->GetPoint(j, &landmarkPoint);
      for (unsigned int d = 0; d < SpaceDimension; ++d)
      {
        landmarkIndex[d] = static_cast<IndexValueType>(itk::Math::Round<std::int64_t>(landmarkPoint[d]));
      }

      /** Compute the input point in physical coordinates and replace the point. */
      if (landmarksInFixedImage)
      {
        fixedImage->TransformIndexToPhysicalPoint(landmarkIndex, landmarkPoint);
      }
      else
      {
        movingImage->TransformIndexToPhysicalPoint(landmarkIndex, landmarkPoint);
      }
      landmarkPointSet->SetPoint(j, landmarkPoint);
    }
  }

  /** Apply initial transform if necessary, for fixed image landmarks only. */
  if (const auto * const initialTransform = this->Superclass1::GetInitialTransform();
      initialTransform != nullptr && landmarksInFixedImage && this->GetUseComposition())
  {
    InputPointType inputPoint{};
    for (unsigned int j = 0; j < nrofpoints; ++j)
    {
      landmarkPointSet->GetPoint(j, &inputPoint);
      inputPoint = initialTransform->TransformPoint(inputPoint);
      landmarkPointSet->SetPoint(j, inputPoint);
    }
  }

} // end ReadLandmarkFile()


/**
 * ************************* ReadFromFile ************************
 */

template <typename TElastix>
void
SplineKernelTransform<TElastix>::ReadFromFile()
{
  const Configuration & configuration = itk::Deref(Superclass2::GetConfiguration());

  /** Read kernel type. */
  std::string kernelType = "unknown";
  bool        skret = configuration.ReadParameter(kernelType, "SplineKernelType", 0);
  if (skret)
  {
    this->SetKernelType(kernelType);
  }
  else
  {
    log::error("ERROR: the SplineKernelType is not given in the transform parameter file.");
    itkExceptionMacro("ERROR: unable to configure transform.");
  }

  /** Interpolating or approximating spline. */
  double splineRelaxationFactor = 0.0;
  configuration.ReadParameter(splineRelaxationFactor, "SplineRelaxationFactor", this->GetComponentLabel(), 0, -1);
  this->m_KernelTransform->SetStiffness(splineRelaxationFactor);

  /** Set the Poisson ratio; default = 0.3 = steel. */
  double poissonRatio = 0.3;
  configuration.ReadParameter(poissonRatio, "SplinePoissonRatio", this->GetComponentLabel(), 0, -1);
  this->m_KernelTransform->SetPoissonRatio(poissonRatio);

  /** Read number of parameters. */
  unsigned int numberOfParameters = 0;
  configuration.ReadParameter(numberOfParameters, "NumberOfParameters", 0);

  /** Read source landmarks. */
  std::vector<CoordinateType> fixedImageLandmarks(numberOfParameters, CoordinateType{});
  bool                        retfil =
    configuration.ReadParameter(fixedImageLandmarks, "FixedImageLandmarks", 0, numberOfParameters - 1, true);
  if (!retfil)
  {
    log::error("ERROR: the FixedImageLandmarks are not given in the transform parameter file.");
    itkExceptionMacro("ERROR: unable to configure transform.");
  }

  /** Convert to fixedParameters type and set in transform. */
  ParametersType fixedParams(numberOfParameters);
  for (unsigned int i = 0; i < numberOfParameters; ++i)
  {
    fixedParams[i] = fixedImageLandmarks[i];
  }
  this->m_KernelTransform->SetFixedParameters(fixedParams);

  /** Call the ReadFromFile from the TransformBase.
   * This must be done after setting the source landmarks and the
   * splinekerneltype, because later the ReadFromFile from
   * TransformBase calls SetParameters.
   */
  this->Superclass2::ReadFromFile();

} // ReadFromFile()


/**
 * ************************* CustomizeTransformParameterMap ************************
 */

template <typename TElastix>
auto
SplineKernelTransform<TElastix>::CreateDerivedTransformParameterMap() const -> ParameterMapType
{
  auto & itkTransform = *m_KernelTransform;

  return { { "SplineKernelType", { m_SplineKernelType } },
           { "SplinePoissonRatio", { Conversion::ToString(itkTransform.GetPoissonRatio()) } },
           { "SplineRelaxationFactor", { Conversion::ToString(itkTransform.GetStiffness()) } },
           { "FixedImageLandmarks", Conversion::ToVectorOfStrings(itkTransform.GetFixedParameters()) } };

} // end CustomizeTransformParameterMap()


} // end namespace elastix

#endif // end #ifndef elxSplineKernelTransform_hxx
