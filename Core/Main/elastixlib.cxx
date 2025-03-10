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

// Elastix header files:
#include "elastixlib.h"
#include "elxElastixMain.h"
#include "elastix.h" // For ConvertSecondsToDHMS and GetCurrentDateAndTime.

#ifdef _ELASTIX_USE_MEVISDICOMTIFF
#  include "itkUseMevisDicomTiff.h"
#endif

// ITK header files:
#include <itkDataObject.h>
#include <itkObject.h>
#include <itkTimeProbe.h>
#include <itksys/SystemInformation.hxx>
#include <itksys/SystemTools.hxx>

// Standard C++ header files:
#include <cassert>
#include <climits> // For UINT_MAX.
#include <iostream>
#include <string>
#include <vector>

namespace elastix
{

/**
 * ******************* Constructor ***********************
 */

ELASTIX::ELASTIX() { assert(BaseComponent::IsElastixLibrary()); }

/**
 * ******************* Destructor ***********************
 */

ELASTIX::~ELASTIX() { assert(BaseComponent::IsElastixLibrary()); }

/**
 * ******************* GetResultImage ***********************
 */

ELASTIX::ConstImagePointer
ELASTIX::GetResultImage() const
{
  return this->m_ResultImage;
} // end GetResultImage()


ELASTIX::ImagePointer
ELASTIX::GetResultImage()
{
  return this->m_ResultImage;
} // end GetResultImage()


/**
 * ******************* GetTransformParameterMap ***********************
 */

ELASTIX::ParameterMapType
ELASTIX::GetTransformParameterMap() const
{
  return this->m_TransformParametersList.back();
} // end GetTransformParameterMap()


/**
 * ******************* GetTransformParameterMapList ***********************
 */

ELASTIX::ParameterMapListType
ELASTIX::GetTransformParameterMapList() const
{
  return this->m_TransformParametersList;
} // end GetTransformParameterMapList()


/**
 * ******************* RegisterImages ***********************
 */

int
ELASTIX::RegisterImages(ImagePointer             fixedImage,
                        ImagePointer             movingImage,
                        const ParameterMapType & parameterMap,
                        const std::string &      outputPath,
                        bool                     performLogging,
                        bool                     performCout,
                        ImagePointer             fixedMask,
                        ImagePointer             movingMask)
{
  std::vector<ParameterMapType> parameterMaps(1);
  parameterMaps[0] = parameterMap;
  return this->RegisterImages(
    fixedImage, movingImage, parameterMaps, outputPath, performLogging, performCout, fixedMask, movingMask);

} // end RegisterImages()


/**
 * ******************* RegisterImages ***********************
 */

int
ELASTIX::RegisterImages(ImagePointer                          fixedImage,
                        ImagePointer                          movingImage,
                        const std::vector<ParameterMapType> & parameterMaps,
                        const std::string &                   outputPath,
                        bool                                  performLogging,
                        bool                                  performCout,
                        ImagePointer                          fixedMask,
                        ImagePointer                          movingMask,
                        ObjectPointer                         transform)
{
  /** Some typedef's. */
  using ElastixMainType = elx::ElastixMain;
  using DataObjectContainerType = ElastixMainType::DataObjectContainerType;
  using DataObjectContainerPointer = ElastixMainType::DataObjectContainerPointer;

  using ArgumentMapType = ElastixMainType::ArgumentMapType;
  using ArgumentMapEntryType = ArgumentMapType::value_type;

  // Clear output transform parameters
  this->m_TransformParametersList.clear();

  /** Some declarations and initialisations. */

  std::string value;

  /** Setup the argumentMap for output path. */
  if (!outputPath.empty())
  {
    /** Put command line parameters into parameterFileList. */
    value = outputPath;

    /** Make sure that last character of the output folder equals a '/'. */
    if (outputPath.back() != '/')
    {
      value.append("/");
    }
  }

  /** Save this information. */
  const auto outFolder = value;

  const ArgumentMapType argMap{ /** The argv0 argument, required for finding the component.dll/so's. */
                                ArgumentMapEntryType("-argv0", "elastix"),
                                ArgumentMapEntryType("-out", outFolder)
  };

  /** Check if the output directory exists. */
  if (performLogging && !itksys::SystemTools::FileIsDirectory(outFolder))
  {
    if (performCout)
    {
      std::cerr << "ERROR: the output directory does not exist.\n"
                << "You are responsible for creating it." << std::endl;
    }
    return -2;
  }

  /** Setup the log system. */
  const std::string     logFileName = performLogging ? (outFolder + "elastix.log") : "";
  const elx::log::guard logGuard{};
  int                   returndummy = elx::log::setup(logFileName, performLogging, performCout) ? 0 : 1;
  if ((returndummy != 0) && performCout)
  {
    if (performCout)
    {
      std::cerr << "ERROR while setting up the log system." << std::endl;
    }
    return returndummy;
  }
  elx::log::info("");

  /** Declare a timer, start it and print the start time. */
  itk::TimeProbe totaltimer;
  totaltimer.Start();
  elx::log::info(std::ostringstream{} << "elastix is started at " << GetCurrentDateAndTime() << ".\n");

  /************************************************************************
   *                                              *
   *  Generate containers with input images       *
   *                                              *
   ************************************************************************/

  /* Allocate and store images in containers */
  auto fixedImageContainer = DataObjectContainerType::New();
  auto movingImageContainer = DataObjectContainerType::New();
  fixedImageContainer->CreateElementAt(0) = fixedImage;
  movingImageContainer->CreateElementAt(0) = movingImage;

  DataObjectContainerPointer                fixedMaskContainer = nullptr;
  DataObjectContainerPointer                movingMaskContainer = nullptr;
  DataObjectContainerPointer                resultImageContainer = nullptr;
  ElastixMainType::FlatDirectionCosinesType fixedImageOriginalDirectionFlat;

  /* Allocate and store masks in containers if available*/
  if (fixedMask)
  {
    fixedMaskContainer = DataObjectContainerType::New();
    fixedMaskContainer->CreateElementAt(0) = fixedMask;
  }
  if (movingMask)
  {
    movingMaskContainer = DataObjectContainerType::New();
    movingMaskContainer->CreateElementAt(0) = movingMask;
  }

  // todo original direction cosin, problem is that Image type is unknown at this in elastixlib.cxx
  // for now in elaxElastixTemplate (Run()) direction cosines are taken from fixed image

  /**
   * ********************* START REGISTRATION *********************
   *
   * Do the (possibly multiple) registration(s).
   */

  const auto nrOfParameterFiles = parameterMaps.size();
  assert(nrOfParameterFiles <= UINT_MAX);

  for (unsigned i{}; i < static_cast<unsigned>(nrOfParameterFiles); ++i)
  {
    /** Create another instance of ElastixMain. */
    const auto elastixMain = ElastixMainType::New();

    /** Set stuff we get from a former registration. */
    elastixMain->SetInitialTransform(transform);
    elastixMain->SetFixedImageContainer(fixedImageContainer);
    elastixMain->SetMovingImageContainer(movingImageContainer);
    elastixMain->SetFixedMaskContainer(fixedMaskContainer);
    elastixMain->SetMovingMaskContainer(movingMaskContainer);
    elastixMain->SetResultImageContainer(resultImageContainer);
    elastixMain->SetOriginalFixedImageDirectionFlat(fixedImageOriginalDirectionFlat);

    /** Set the current elastix-level. */
    elastixMain->SetElastixLevel(i);
    elastixMain->SetTotalNumberOfElastixLevels(nrOfParameterFiles);

    /** Print a start message. */
    elx::log::info(std::ostringstream{} << "-------------------------------------------------------------------------\n"
                                        << '\n'
                                        << "Running elastix with parameter map " << i);

    /** Declare a timer, start it and print the start time. */
    itk::TimeProbe timer;
    timer.Start();
    elx::log::info(std::ostringstream{} << "Current time: " << GetCurrentDateAndTime() << ".");

    /** Start registration. */
    returndummy = elastixMain->Run(argMap, parameterMaps[i]);

    /** Check for errors. */
    if (returndummy != 0)
    {
      elx::log::error("Errors occurred!");
      return returndummy;
    }

    /** Get the transform, the fixedImage and the movingImage
     * in order to put it in the (possibly) next registration.
     */
    transform = elastixMain->GetModifiableFinalTransform();
    fixedImageContainer = elastixMain->GetModifiableFixedImageContainer();
    movingImageContainer = elastixMain->GetModifiableMovingImageContainer();
    fixedMaskContainer = elastixMain->GetModifiableFixedMaskContainer();
    movingMaskContainer = elastixMain->GetModifiableMovingMaskContainer();
    resultImageContainer = elastixMain->GetModifiableResultImageContainer();
    fixedImageOriginalDirectionFlat = elastixMain->GetOriginalFixedImageDirectionFlat();

    /** Stop timer and print it. */
    timer.Stop();
    elx::log::info(std::ostringstream{} << "\nCurrent time: " << GetCurrentDateAndTime() << ".\n"
                                        << "Time used for running elastix with this parameter file: "
                                        << ConvertSecondsToDHMS(timer.GetMean(), 1) << ".\n");

    /** Get the transformation parameter map. */
    this->m_TransformParametersList.push_back(elastixMain->GetTransformParameterMap());

  } // end loop over registrations

  elx::log::info("-------------------------------------------------------------------------\n");

  /** Stop totaltimer and print it. */
  totaltimer.Stop();
  elx::log::info(std::ostringstream{} << "Total time elapsed: " << ConvertSecondsToDHMS(totaltimer.GetMean(), 1)
                                      << ".\n");

  /************************************************************************
   *                                *
   *  Cleanup everything            *
   *                                *
   ************************************************************************/

  /*
   *  Make sure all the components that are defined in a Module (.DLL/.so)
   *  are deleted before the modules are closed.
   */

  /* Set result image for output */
  if (resultImageContainer.IsNotNull() && resultImageContainer->Size() > 0 &&
      resultImageContainer->ElementAt(0).IsNotNull())
  {
    this->m_ResultImage = resultImageContainer->ElementAt(0);
  }

  transform = nullptr;
  fixedImageContainer = nullptr;
  movingImageContainer = nullptr;
  fixedMaskContainer = nullptr;
  movingMaskContainer = nullptr;
  resultImageContainer = nullptr;

  /** Exit and return the error code. */
  return 0;

} // end RegisterImages()


} // end namespace elastix
