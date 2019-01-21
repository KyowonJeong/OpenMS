//
// Created by JihyungKim on 07.12.18.
//

#include <OpenMS/config.h>
#include <OpenMS/APPLICATIONS/TOPPBase.h>
#include <OpenMS/CONCEPT/Types.h>
#include <OpenMS/FORMAT/FileTypes.h>
#include <OpenMS/FORMAT/MzMLFile.h>
#include <queue>
#include "boost/dynamic_bitset.hpp"
#include <iostream>

using namespace OpenMS;
using namespace std;

class TDOnlineDeconv:
        public TOPPBase
{

public:
    TDOnlineDeconv():
            TOPPBase("TDOnlineDeconv", "Online Deconvolution for Smart MS2 acquisition with top down data", false)\
    {}

    typedef pair<double, pair<Size, Size>> ppi;

    typedef struct LogMzPeak{
        Peak1D *orgPeak;
        double logMz;
        int charge;
        int isotopeIndex;
        double score;

        LogMzPeak(): orgPeak(nullptr), logMz(-10000), charge(0){}
        LogMzPeak(Peak1D &peak): orgPeak(&peak), logMz(getLogMz(peak.getMZ())), charge(0){}

        double getMass(){
            return exp(logMz) * charge;
        }
        bool operator<(const LogMzPeak &a){
            return logMz < a.logMz;
        }
    };


protected:

    static double getLogMz(double mz){
        const double protonMass = 1.0072764668;// 1.0072765;
        return log(mz - protonMass);
    }



    void registerOptionsAndFlags_() override {
        registerInputFile_("in", "<file>", "", "Input file.");
        setValidFormats_("in", ListUtils::create<String>("mzML"));
    }
    ExitCodes main_(int, const char **) override
    {
        //-------------------------------------------------------------
        // parsing parameters
        //-------------------------------------------------------------
//        String infilePath = getStringOption_("in");
        String infilePath = "/Users/kyowonjeong/Documents/A4B/mzml/05-26-17_B7A_yeast_td_fract12_rep1.mzML";
       // String infilePath = "/Users/kyowonjeong/Documents/A4B/mzml/180523_Cytocrome_C_MS2_HCD.mzML";
       //  String infilePath = "/Users/kyowonjeong/Documents/A4B/mzml/180523_Myoglobin_MS2_HCD.mzML";
        cout << "file name : " << infilePath << endl;
        // just for quick use

        //-------------------------------------------------------------
        // reading input
        //-------------------------------------------------------------

        MzMLFile mzml;
        mzml.setLogType(log_type_);

        // load input
        MSExperiment map;
        mzml.load(infilePath, map);
        cout << "Loaded consensus maps" << endl;
        clock_t begin = clock();
        int cntr = onlineDeonvolution(map);
        clock_t end = clock();
        double elapsed_secs = double(end - begin) / CLOCKS_PER_SEC;
        std::cout << elapsed_secs << " seconds elapsed" << std::endl;
        std::cout << elapsed_secs/cntr*1000 << " msec per spectrum"<< std::endl;

        return EXECUTION_OK;
    }

    int onlineDeonvolution(MSExperiment &map){
        double threshold = 5000;
        int filterSize = 15;
        int minCharge = 5;
        int minChargePeakCount = filterSize*0.3; // inclusive
        int massCount = 5;
        double tolerance = 5e-6; // 5 ppm

        int specCntr = 0, qspecCntr = 0;
        double filter[filterSize];

        for(int i=0;i<filterSize;i++){
            filter[i] = log(1.0/(i + minCharge)); // should be descending, and negative!
        }

        ofstream mfile;
        mfile.open ("/Users/kyowonjeong/Documents/A4B/matlab/bins.m");


        for (auto it = map.begin(); it != map.end(); ++it) {
            if (it->getMSLevel() != 1) continue;
           // if(it->size()<1000) continue;
            specCntr++;

            double rt = it->getRT();

            vector<LogMzPeak> logMzPeaks;
            logMzPeaks.reserve(it->size());
            for (auto &peak : *it){
                if(peak.getIntensity() <= threshold) continue;
                LogMzPeak logMzPeak(peak);
                logMzPeaks.push_back(logMzPeak);
            }

            auto peakGroups =
                    findPeakGroups(logMzPeaks, filter, filterSize, minCharge, tolerance, minChargePeakCount, massCount, mfile, specCntr);
            if (peakGroups.empty())
                continue;
            std::map<double, double> scoreMassMap;

            for(auto &pg : peakGroups){
                double* perChargeIntensities = new double[filterSize];
                double* perIsotopeIntensities = new double[10]; // tentative

                fill_n(perChargeIntensities,filterSize,0);
                fill_n(perIsotopeIntensities,10,0);

                for(auto &p : pg){
                    int index = p.charge - minCharge;
                    perChargeIntensities[index] += p.orgPeak->getIntensity();
                    if(p.isotopeIndex<10)
                        perIsotopeIntensities[p.isotopeIndex] += p.orgPeak->getIntensity();
                }

                int maxIntensityIndex;
                int firstNonZeroIndex = -1;
                int lastNonZeroIndex = filterSize;
                double maxIntensity = -1;

                for(int i=0;i<filterSize;i++) {
                    if(perChargeIntensities[i]!=0){
                        lastNonZeroIndex = i;
                        if(firstNonZeroIndex<0 ) firstNonZeroIndex=i;
                    }

                    if (maxIntensity < perChargeIntensities[i]) {
                        maxIntensity = perChargeIntensities[i];
                        maxIntensityIndex = i;
                    }
                }

                double score = 0;
                for(int k=maxIntensityIndex;k<lastNonZeroIndex;k++){
                    if(k>=filterSize-1) break;
                    score += getLogLikelihoodRatioScore(perChargeIntensities[k], perChargeIntensities[k+1]);
                }
                for(int k=maxIntensityIndex;k>firstNonZeroIndex;k--){
                    if(k<=0) break;
                    score += getLogLikelihoodRatioScore(perChargeIntensities[k], perChargeIntensities[k-1]);
                }
                //continue;
                //cout<<maxIntensityIndex<<endl;

                if(score > 0 && !pg.empty()){
                    scoreMassMap[score] = pg[0].getMass();
                    //cout<<specCntr<<" "<< pg[0].getMass() <<endl;
                    for(int i=0;i<10;i++){
                       // cout<<perIsotopeIntensities[i]<<" ";
                    }
                    //cout<<endl;

                     /*cout<<"s{"<<qspecCntr<<"}=[";
                    for(int i=0;i<filterSize;i++){
                        cout<<perChargeIntensities[i]<<",";
                    }

                    cout<<"];"<<endl;*/
                }

            }

            if(scoreMassMap.empty()) continue;
            auto iter = scoreMassMap.rbegin();
            qspecCntr++;

            for (int i=0; i<20 && i<scoreMassMap.size() ; ++iter, i++){
                cout << qspecCntr<<" "<<specCntr<<" "<<iter->first << " " << iter->second << " " << peakGroups.size() << " " << scoreMassMap.size()<< endl;
            }
            cout << endl;
        }
        mfile.close();
        return specCntr;
    }

    /*
    vector<Peak1D> filterSpectrum(MSSpectrum &spectrum, double window, double factor, double th){
        deque<Peak1D> peaksInWindow;
        vector<Peak1D> filtered;
        vector<double> intensityHeap;
        intensityHeap.reserve(spectrum.size());
        filtered.reserve(spectrum.size());
        int wsIndex = 0, weIndex = 0;
        double w = window/2;
        double prevMedian = 0;
        vector<Peak1D> initFiltered;
        initFiltered.reserve(spectrum.size());
        for(auto &p : spectrum) if(p.getIntensity() > th) initFiltered.push_back(p);

        for (int i=0;i<initFiltered.size();i++) {
            auto p = initFiltered[i];
            auto mz = p.getMZ();
            double median = prevMedian;
            while(initFiltered[wsIndex].getMZ() < mz - w){
                auto firstp = peaksInWindow.front();
                auto j = lower_bound(intensityHeap.begin(), intensityHeap.end(), firstp.getIntensity());
                intensityHeap.erase(j);
                // find firstp in heap and remove it using binary search
                peaksInWindow.pop_front();
                wsIndex++;
            }
            while(weIndex< initFiltered.size() && initFiltered[weIndex].getMZ() < mz + w){
                auto lastp = spectrum[weIndex];
                peaksInWindow.push_back(lastp);
                auto j = lower_bound(intensityHeap.begin(), intensityHeap.end(), lastp.getIntensity());
                intensityHeap.insert(j, lastp.getIntensity());
                median = intensityHeap[intensityHeap.size()/2];
                weIndex++;
            }
            if(p.getIntensity() >= median * factor)
                filtered.push_back(p);

            prevMedian = median;
        }
        return filtered;
    }*/

    vector<vector<LogMzPeak>> findPeakGroups(vector<LogMzPeak> &logMzPeaks, double *filter, int filterSize, int minCharge,
                                             double tol, int minChargePeakCount, int massCount, ofstream& mfile, int specCntr){
        vector<vector<LogMzPeak>> peakGroups;
        // minChargePeakCount should be < filterSize
        double min = logMzPeaks[0].logMz - filter[0]; // never fix it..
        double max = logMzPeaks[logMzPeaks.size()-1].logMz - filter[filterSize-minChargePeakCount];
        peakGroups.reserve(massCount*100);

        int binNumber = (max-min)/tol;
        if(binNumber <=0) return peakGroups;
        boost::dynamic_bitset<> mzBins(binNumber + 1);

        for (auto &p : logMzPeaks) {
            int bi = (p.logMz - logMzPeaks[0].logMz) / tol;
            if (bi >= binNumber) break;
            mzBins[bi] = true;
        }
        mzBins = mzBins>>1 | mzBins;

        if(mzBins.count() == 0) return peakGroups;

        boost::dynamic_bitset<> massBins(binNumber + 1);

        Byte* massBinScores = new Byte[binNumber + 1];
        fill_n(massBinScores, binNumber + 1,0);

        int binOffsets[filterSize];
        for(int i=0;i<filterSize;i++){
            binOffsets[i] = (filter[0] - filter[i]) / tol;
        }

        int setMassBinIndex = mzBins.find_first();

        while(setMassBinIndex != mzBins.npos){
            for(int j=0;j<filterSize;j++) {
                int bi = (setMassBinIndex + binOffsets[j]);
                if (bi >= binNumber) break;
                massBinScores[bi]++;

                if (massBinScores[bi] >= minChargePeakCount)
                    massBins[bi] = true;
            }
            setMassBinIndex = mzBins.find_next(setMassBinIndex);
        }

        if(massBins.count() == 0) return peakGroups;

        double isotopeMass = 1.003355;
        int isoMinCntr = 4;
        int isoMaxCntr = 8;
        setMassBinIndex = massBins.find_first();

        while(setMassBinIndex != massBins.npos){
            auto isoPresent = true;
            auto m = exp(min + setMassBinIndex * tol);
            int* isoBins = new int[isoMaxCntr-1];
            fill_n(isoBins, isoMaxCntr-1,0);

            for(int i=1;i<isoMaxCntr;i++) {
                int bin = setMassBinIndex + isotopeMass*i/m/tol;
                if(bin>=binNumber){
                    isoPresent = false;
                    break;
                }
                isoBins[i-1] = bin;
                if(i<isoMinCntr) isoPresent &= massBins[bin];
                if(!massBins[bin]) break;
            }

            if(isoPresent){
                for(int i=1;i<isoMaxCntr;i++) {
                    int bin = isoBins[i-1];
                    if(bin == 0) break;
                    massBinScores[setMassBinIndex] = massBinScores[setMassBinIndex] > massBinScores[bin] ? massBinScores[setMassBinIndex] : massBinScores[bin];
                    massBins[bin] = false;
                    if(bin > 0) massBins[bin-1] = false;
                }
                if(setMassBinIndex > 0) massBins[setMassBinIndex-1] = false;
            }else{
                massBins[setMassBinIndex] = false;
            }
            setMassBinIndex = massBins.find_next(setMassBinIndex);
        }

        if(massBins.count() == 0) return peakGroups;

        int* binScoreDist = new int[filterSize + 1];
        fill_n(binScoreDist,filterSize,0);

        setMassBinIndex = massBins.find_first();
        while(setMassBinIndex != massBins.npos){
           // int j = massBinScores[index] >= filterSize? filterSize - 1 : massBinScores[index];
            binScoreDist[massBinScores[setMassBinIndex]]++;
            setMassBinIndex = massBins.find_next(setMassBinIndex);
        }
        if(massBins.count() == 0) return peakGroups;

        int binScoreThreshold = minChargePeakCount, tsum=0;
        for(int i=filterSize;i>=0;i--){
            tsum+=binScoreDist[i];
            if(tsum >= massCount){
                binScoreThreshold = i > binScoreThreshold ? i : binScoreThreshold;
                break;
            }
        }

        int* currentPeakIndex = new int[filterSize];
        fill_n(currentPeakIndex,filterSize,0);

        setMassBinIndex = massBins.find_first();
        while(setMassBinIndex != massBins.npos){
            if(massBinScores[setMassBinIndex]<binScoreThreshold){
                setMassBinIndex = massBins.find_next(setMassBinIndex);
                continue;
            }
            map<int, int> peakIndexToChargeMap; // index - > charge
            map<int, int> peakIndexToIsotopeIndexMap;
            int isoOff = 0;

            for(int j=0;j<filterSize;j++) {
                int charge = j + minCharge;
                while(currentPeakIndex[j] < logMzPeaks.size()) {
                    double logMz = logMzPeaks[currentPeakIndex[j]].logMz;
                    int bi = (logMz - min - filter[j]) / tol;
                    if (bi>setMassBinIndex) break;
                    if (setMassBinIndex == bi) {
                        peakIndexToChargeMap[currentPeakIndex[j]] = charge;
                        peakIndexToIsotopeIndexMap[currentPeakIndex[j]] = 0;

                        if(currentPeakIndex[j]>0){
                            if(logMz - logMzPeaks[currentPeakIndex[j]-1].logMz < tol){
                                peakIndexToChargeMap[currentPeakIndex[j]-1] = charge;
                                peakIndexToIsotopeIndexMap[currentPeakIndex[j]-1] = 0;
                            }
                        }
                        int currentPeakIndexForIsotopes = currentPeakIndex[j] + 1;

                        for(int i=1;i<isoMaxCntr;i++) {
                            double logMzIsotope = log(exp(logMz) + isotopeMass*i/charge);

                            bool isotopePeakPresent = false;
                            while(currentPeakIndexForIsotopes < logMzPeaks.size()){
                                double logMzForIsotope = logMzPeaks[currentPeakIndexForIsotopes].logMz;
                                currentPeakIndexForIsotopes++;
                                if(logMzForIsotope < logMzIsotope - tol) continue;
                                if(logMzForIsotope > logMzIsotope + tol) break;

                                isotopePeakPresent = true;

                                peakIndexToChargeMap[currentPeakIndexForIsotopes - 1] = charge;
                                peakIndexToIsotopeIndexMap[currentPeakIndexForIsotopes - 1] = i;

                            }
                            if(!isotopePeakPresent) break;
                        }

                        currentPeakIndexForIsotopes = currentPeakIndex[j] - 1;

                        for(int i=-1;i>-isoMaxCntr;i--) {
                            double logMzIsotope = log(exp(logMz) + isotopeMass*i/charge);

                            bool isotopePeakPresent = false;
                            while(currentPeakIndexForIsotopes >= 0){
                                double logMzForIsotope = logMzPeaks[currentPeakIndexForIsotopes].logMz;
                                currentPeakIndexForIsotopes--;
                                if(logMzForIsotope < logMzIsotope - tol) break;
                                if(logMzForIsotope > logMzIsotope + tol) continue;

                                isotopePeakPresent = true;

                                peakIndexToChargeMap[currentPeakIndexForIsotopes + 1] = charge;
                                peakIndexToIsotopeIndexMap[currentPeakIndexForIsotopes + 1] = i;
                                isoOff = i > isoOff? isoOff : i;
                            }
                            if(!isotopePeakPresent) break;
                        }
                    }
                    currentPeakIndex[j]++;
                }
            }
            vector<LogMzPeak> logpeaks;
            logpeaks.reserve(peakIndexToChargeMap.size());

            for(auto iter = peakIndexToChargeMap.begin(); iter != peakIndexToChargeMap.end(); ++ iter){
                auto t = iter->first;
                LogMzPeak p(*logMzPeaks[t].orgPeak);
                p.charge = iter->second;
                p.isotopeIndex = peakIndexToIsotopeIndexMap[t] - isoOff;
                logpeaks.push_back(p);
            }

            peakGroups.push_back(logpeaks);
            setMassBinIndex = massBins.find_next(setMassBinIndex);
        }
        //peakGroups.clear();
        return peakGroups;
    }

/*
    void sortMatrix(vector<vector<LogMzPeak>> &matrix, vector<LogMzPeak> &result){
        priority_queue< ppi, vector<ppi>, greater<ppi> > pq;

        for (Size i=0; i< matrix.size(); i++){
            pq.push({matrix[i][0].logMz, {i, 0}});
        }

        while (!pq.empty()) {
            ppi curr = pq.top();
            pq.pop();

            // i ==> Array Number (filter index)
            // j ==> Index in the array number (peak index)
            Size i = curr.second.first;
            Size j = curr.second.second;

            result.push_back(matrix[i][j]);

            // The next element belongs to same array as current.
            if (j + 1 < matrix[i].size())
                pq.push({ matrix[i][j + 1].logMz, { i, j + 1 } });
        }
    }
*/
    double getLogLikelihoodRatioScore(double int1, double int2){
        double sScore = getLogLikelihood(int1, int2, false);
        double nScore = getLogLikelihood(int1, int2, true);
       // cout<<int1 << " " << int2 << " " << pow(10, sScore) << " " << pow(10, nScore) << " " << sScore - nScore << endl;
        return sScore - nScore;
    }

    double getLogLikelihood(double int1, double int2, bool isH0){
        double tmp = 1e-1;
        double ret;
        if(int1<=0){
            if(int2<=0){
                if(isH0) ret = .8;
                else
                    ret = .9;
            }else{
                if(isH0) ret = .2;
                else
                    ret = .1;
            }
        }else{
            if(int2<=0){
                if(isH0) ret = .8;
                else ret = .4;
            }else if(int1<int2){
                if(isH0) ret = .1;
                else ret = .1;
            }else {
                if (isH0) ret = .1;
                else ret = .5;
            }
        }
        return log10(ret);
    }
};

int main(int argc, const char** argv)
{
    TDOnlineDeconv tool;
    return tool.main(argc, argv);
}
