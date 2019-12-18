// --------------------------------------------------------------------------
//                   OpenMS -- Open-Source Mass Spectrometry
// --------------------------------------------------------------------------
// Copyright The OpenMS Team -- Eberhard Karls University Tuebingen,
// ETH Zurich, and Freie Universitaet Berlin 2002-2018.
//
// This software is released under a three-clause BSD license:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of any author or any participating institution
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
// For a full list of authors, refer to the file AUTHORS.
// --------------------------------------------------------------------------
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL ANY OF THE AUTHORS OR THE CONTRIBUTING
// INSTITUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// --------------------------------------------------------------------------
// $Maintainer: Kyowon Jeong, Jihyung Kim $
// $Authors: Kyowon Jeong, Jihyung Kim $
// --------------------------------------------------------------------------

#pragma once

#include <OpenMS/APPLICATIONS/TOPPBase.h>
#include <OpenMS/KERNEL/Peak1D.h>
#include <OpenMS/CONCEPT/Types.h>
#include <OpenMS/CHEMISTRY/ISOTOPEDISTRIBUTION/IsotopeDistribution.h>
#include <OpenMS/CHEMISTRY/ISOTOPEDISTRIBUTION/CoarseIsotopePatternGenerator.h>
#include <OpenMS/FILTERING/DATAREDUCTION/MassTraceDetection.h>

namespace OpenMS
{

  /**
   * @brief Wrapper struct for all the structs needed by the FLASHDeconv
   *
   * @see FLASHDeconv
   * @reference: FeatureFinderAlgorithmPickedHelperStructs
   */

  struct OPENMS_DLLAPI FLASHDeconvHelperStructs
  {
    struct OPENMS_DLLAPI Parameter
    {
        int minCharge;
        double minMass;
        double maxMass;
        double currentMaxMass;
        double tolerance;
        String fileName;// up to here: ordinary user accessible parameters

        double intensityThreshold;// advanced parameters
        double minIsotopeCosine;
        double minChargeCosine;

        double tolerance2;
        double minIsotopeCosineSpec2;
        double minChargeCosineSpec2;
        int minContinuousChargePeakCount2;

        double minIsotopeCosineSpec;
        double minChargeCosineSpec;

        int minContinuousChargePeakCount;
        int maxIsotopeCount;
        int maxMassCount;
        unsigned int maxMSLevel = 1;//maxMSL;
        //double charg = 1eDistributionScoreThreshold;
        double RTwindow;
        double minRTSpan;
        std::vector<int> hCharges{2, 3, 5,}; // automated or fixed parameters
        int chargeRange;
        int currentChargeRange;
        double binWidth;
        double binWidth2;
        int minNumOverLappedScans = 1;
        int numOverlappedScans = minNumOverLappedScans;
        int threads = 1;
        int writeSpecTsv = 0;
        //int jitter = 0;
    };

    struct OPENMS_DLLAPI PrecalcularedAveragine
    {
      std::vector<IsotopeDistribution> isotopes;
      std::vector<double> norms;
      std::vector<double> averageMassDelta;
      std::vector<Size> leftIndices;
      std::vector<Size> rightIndices;

      double massInterval;
      double minMass;

      PrecalcularedAveragine(double m, double M, double delta, CoarseIsotopePatternGenerator *generator);
      IsotopeDistribution get(double mass);
      double getNorm(double mass);
      Size getLeftIndex(double mass);
      Size getRightIndex(double mass);
      double getAverageMassDelta(double mass);

    };


    struct OPENMS_DLLAPI LogMzPeak
    {
      //Peak1D *orgPeak;
      double mz = 0;
      double intensity = 0;
      double logMz = 0;
      double mass = .0;
      int charge = 0;
      int isotopeIndex = -1;
      //double score;

      LogMzPeak();

      explicit LogMzPeak(Peak1D &peak);

      LogMzPeak(LogMzPeak &peak, int c, int i);

      ~LogMzPeak();

      double getUnchargedMass();

      bool operator<(const LogMzPeak &a) const;
      bool operator>(const LogMzPeak &a) const;
    };

    struct OPENMS_DLLAPI PeakGroup
    {
      std::vector<LogMzPeak> peaks;
      double monoisotopicMass = .0;
      double avgMass = .0;
      double intensity = .0;
      Size massBinIndex = 0;
      // double chargeDistributionScore = .0;

      double isotopeCosineScore = .0;
      double chargeCosineScore = .0;
      int massIndex, specIndex, massCntr;
      int maxCharge, minCharge;

      MSSpectrum *spec;

      ~PeakGroup();
      void push_back(LogMzPeak &p);
      void reserve(Size n);

      bool operator<(const PeakGroup &a) const;
      bool operator>(const PeakGroup &a) const;

      bool operator==(const PeakGroup &a) const;

      void updateMassesAndIntensity(PrecalcularedAveragine &averagines, int offset = 0, int maxIsoIndex = 0);
    };

    static double getLogMz(double mz);
  };
}


