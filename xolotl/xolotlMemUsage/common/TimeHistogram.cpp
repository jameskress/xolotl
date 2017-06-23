#ifndef XMEMUSAGE_TIME_HISTOGRAM_CPP
#define XMEMUSAGE_TIME_HISTOGRAM_CPP

// Definitions of TimeHistogram template class methods.

#include "xolotlMemUsage/common/TimeHistogram.h"

namespace xolotlMemUsage {


template<typename SType, typename RType, typename MType, typename ClockType>
TimeHistogram<SType, RType, MType, ClockType>::TimeHistogram(unsigned int _nBins,
                                DType _binWidth,
                                TSType _startTimestamp)
  : nBins(_nBins),
    binWidth(_binWidth),
    startTimestamp(_startTimestamp),
    bins(nBins)
{
    if((nBins % 2) != 0)
    {
        std::ostringstream estr;
        estr << "Number of bins must be even; " << nBins << " bins requested.";
        throw std::runtime_error(estr.str());
    }
}


template<typename SType, typename RType, typename MType, typename ClockType>
unsigned int
TimeHistogram<SType, RType, MType, ClockType>::FindBin(const TSType& timestamp)
{
    return static_cast<unsigned int>(std::floor((timestamp - startTimestamp)/binWidth));
}


template<typename SType, typename RType, typename MType, typename ClockType>
void
TimeHistogram<SType, RType, MType, ClockType>::HandleSample(const TSType& timestamp, const SType& sample)
{
    assert(timestamp >= startTimestamp);

    // Ensure that the newly-arrived sample falls into one of our current bins.
    unsigned int binIdx = FindBin(timestamp);
    while(binIdx >= nBins)
    {
        Fold();
        binIdx = FindBin(timestamp);
    }
    bins[binIdx].HandleSample(sample);
}



template<typename SType, typename RType, typename MType, typename ClockType>
void
TimeHistogram<SType, RType, MType, ClockType>::Fold(void)
{
    auto oldBinIter = bins.begin();
    auto newBinIter = bins.begin();
    while(oldBinIter != bins.end())
    {
        // Find end of old bins to consider.
        auto nextOldBinIter = oldBinIter + 2;

        // Fold bin at oldBinIter and oldBinIter + 1 into bin at newBinIter.
        newBinIter->SetToCombinationOf(oldBinIter, nextOldBinIter);

        // advance to next pair of input bins and next output bin.
        oldBinIter = nextOldBinIter;
        ++newBinIter;
    }

    // Reset values in second half of histogram.
    while(newBinIter != bins.end())
    {
        newBinIter->Reset();
        ++newBinIter;
    }

    // Reset our notion of the current bin and bin width.
    binWidth *= 2;
}


template<typename SType, typename RType, typename MType, typename ClockType>
void
TimeHistogram<SType, RType, MType, ClockType>::BinData::Reset(void)
{
    runningValue = 0;
    nSamples = 0;
}


// Default implementation suitable for scalar sample types, scalar running
// data types, and scalar aggregated metric types.
template<typename SType, typename RType, typename MType, typename ClockType>
void
TimeHistogram<SType, RType, MType, ClockType>::BinData::SetToCombinationOf(typename std::vector<BinData>::const_iterator firstIter,
                    typename std::vector<BinData>::const_iterator lastIter)
{
    // Check whether we are being asked to combine ourself with other bin(s).
    // I.e., we are the first bin.
    bool involved = (&(*firstIter) == this);

    // Reset ourself if we are not involved in the combination.
    // (If we are, we need to retain our values.)
    if(not involved)
    {
        Reset();    
    }

    // Determine actual first bin that we will need to combine.
    // It is the one after us if we are involved, or the given
    // bin if we aren't involved.
    auto actualFirstIter = (involved ? ++firstIter : firstIter);

    // Combine the combination of (the rest of) the bins.
    for(auto currIter = actualFirstIter; currIter != lastIter; ++currIter)
    {
        runningValue += currIter->runningValue;
        nSamples += currIter->nSamples;
    }
}


// Default implementation suitable for scalar sample types, scalar running
// data types, and scalar aggregated metric types.
template<typename SType, typename RType, typename MType, typename ClockType>
MType
TimeHistogram<SType, RType, MType, ClockType>::BinData::GetMetricValue(void) const
{
    MType ret(0);

    if(nSamples > 0)
    {
        ret = static_cast<MType>(runningValue) / nSamples;
    }
    return ret;
}


template<typename SType, typename RType, typename MType, typename ClockType>
void
TimeHistogram<SType, RType, MType, ClockType>::BinData::HandleSample(const SType& s)
{
    runningValue += s;    
    nSamples++;
}

} // namespace xolotlMemUsage

#endif // XMEMUSAGE_TIME_HISTOGRAM_CPP