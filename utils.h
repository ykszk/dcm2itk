#ifndef UTILS_H
#define UTILS_H
#include <gdcmReader.h>

std::string get_string(const gdcm::DataSet& dataset, const gdcm::Tag& tag);

namespace tags
{
  extern gdcm::Tag modality;
  extern gdcm::Tag pharma;
  extern gdcm::Tag weight;
  extern gdcm::Tag seriesdate;
  extern gdcm::Tag seriestime;
  extern gdcm::Tag pharma_starttime;
  extern gdcm::Tag dose;
  extern gdcm::Tag halflife;

  extern gdcm::Tag rescale_intercept;
  extern gdcm::Tag rescale_slope;
}

double calculate_bw_factor(const gdcm::File& file, bool verbose=false);

/// <summary>
/// Rescale RescaleSlope in the dicom. i.e. new_slope = factor * original_slope
/// </summary>
/// <param name="dcm"></param>
/// <param name="factor"></param>
void rescale_slope(gdcm::File& dcm, double factor);


#endif /* UTILS_H */