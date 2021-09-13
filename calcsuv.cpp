#include "itkImage.h"
#include "itkGDCMImageIO.h"
#include "itkGDCMSeriesFileNames.h"
#include <itkImageIOFactory.h>
#include "itkImageSeriesReader.h"
#include "itkImageFileWriter.h"
#include <gdcmReader.h>
#include <gdcmImageReader.h>
#include <gdcmWriter.h>
#include "gdcmStringFilter.h"
#include <filesystem>
#include <tclap/CmdLine.h>
#include <cctype>
#include "config.h"

namespace fs = std::filesystem;
using std::cerr;
using std::cout;
using std::endl;

std::string get_string(const gdcm::DataSet &dataset, const gdcm::Tag &tag)
{
  auto elm = dataset.GetDataElement(tag);
  std::string str(elm.GetByteValue()->GetPointer(), elm.GetVL());
  return str;
}

namespace tags
{
  gdcm::Tag pharma(0x0054, 0x0016);
  gdcm::Tag weight(0x0010, 0x1030);
  gdcm::Tag seriesdate(0x0008, 0x0021);
  gdcm::Tag seriestime(0x0008, 0x0031);
  gdcm::Tag pharma_starttime(0x0018, 0x1072);
  gdcm::Tag dose(0x0018, 0x1074);
  gdcm::Tag halflife(0x0018, 0x1075);

  gdcm::Tag rescale_intercept(0x0028, 0x1052);
  gdcm::Tag rescale_slope(0x0028, 0x1053);
}

std::string format_date(const std::string &str_date)
{
  auto year = str_date.substr(0, 4);
  auto month = str_date.substr(4, 2);
  auto day = str_date.substr(6, 2);
  return year + '/' + month + '/' + day;
}
std::string format_time(const std::string &str_time)
{
  auto hour = str_time.substr(0, 2);
  auto minute = str_time.substr(2, 2);
  auto second = str_time.substr(4, 2);
  return hour + ':' + minute + ':' + second;
}
time_t datetime2time_t(const std::string &date, const std::string &time)
{
  std::tm tm;
  std::stringstream ss(format_date(date) + ' ' + format_time(time));
  ss >> std::get_time(&tm, "%Y/%m/%d %H:%M:%S");
  return std::mktime(&tm);
}

int main(int argc, char *argv[])
{
  Args args;
  args.ext = ".nii.gz"; // default extension

  try
  {
    TCLAP::CmdLine cmd("Calculate SUV", ' ', PROJECT_VERSION);
    TCLAP::UnlabeledValueArg<std::string> inputDir("input", "Input directory or zip file containing dicom files", true, "", "input");
    cmd.add(inputDir);
    TCLAP::ValueArg<std::string> outdir("", "output", "(optional) Output dicom filename.", false, "", "filename");
    cmd.add(output);
    TCLAP::SwitchArg verboseSwitch("","verbose","Activate verbose mode", cmd, false);


    cmd.parse(argc, argv);
    auto input_filename = inputDir.getValue();
    gdcm::Reader reader;
    reader.SetFileName(input_filename.c_str());
    if (!reader.Read())
    {
      std::cerr << "Could not read: " << input_filename << std::endl;
      return 1;
    }
    auto dcm = reader.GetFile();
    auto dataset = dcm.GetDataSet();
    auto data = dataset.GetDataElement(tags::pharma);
    auto seq = data.GetValueAsSQ();
    if (seq->GetNumberOfItems() != 1)
    {
      throw "Invalid number of items in pharma info";
    }
    auto item = seq->GetItem(1);

    auto pharma_ds = item.GetNestedDataSet();
    auto elm = pharma_ds.GetDataElement(tags::dose);
    auto dose = get_string(pharma_ds, tags::dose);
    auto halflife = get_string(pharma_ds, tags::halflife);
    auto pharma_starttime = get_string(pharma_ds, tags::pharma_starttime);
    auto seriesdate = get_string(dataset, tags::seriesdate);
    auto seriestime = get_string(dataset, tags::seriestime);
    auto weight = std::stoi(get_string(dataset, tags::weight));
    auto series_datetime = datetime2time_t(seriesdate, seriestime);
    auto pharma_datetime = datetime2time_t(seriesdate, pharma_starttime);
    auto decay_time = (series_datetime - pharma_datetime);
    auto decayed_dose = std::stof(dose) * pow(2, -decay_time / std::stoi(halflife));
    auto SUVbwScaleFactor = weight * 1000 / decayed_dose;
    if (verboseSwitch)
    {
      cout << "weight, " << weight << '\n';
      cout << "dose, " << dose << '\n';
      cout << "halflife, " << halflife << '\n';
      cout << "starttime, " << pharma_starttime << '\n';
      cout << "seriesdate, " << seriesdate << '\n';
      cout << "seriestime, " << seriestime << '\n';
      cout << "pharma_starttime, " << pharma_starttime << '\n';
      cout << "scan datetime, " << std::ctime(&series_datetime);
      cout << "pharma datetime, " << std::ctime(&pharma_datetime);
      cout << "decay time" << decay_time << endl;
      cout << "decayed dose" << decayed_dose << endl;
      cout << "SUVbwScaleFactor " << SUVbwScaleFactor << endl;
      cout << endl;
    }
    auto output_filename = output.getValue();
    if (output.isSet())
    {
      auto intercept = std::stof(get_string(dataset, tags::rescale_intercept));
      auto slope = std::stof(get_string(dataset, tags::rescale_slope));
      auto scaled_slope = SUVbwScaleFactor * slope;
      auto elm = dataset.GetDataElement(tags::rescale_slope);
      auto str_scaled_slope = std::to_string(scaled_slope);
      elm.SetByteValue(str_scaled_slope.c_str(), str_scaled_slope.length());
      dataset.Replace(elm);
      cout << get_string(dataset, tags::rescale_slope) << endl;
      dcm.SetDataSet(dataset);
      gdcm::Writer writer;
      writer.SetFileName(output.getValue());
      writer.SetFile(dcm);
      writer.Write();
    }
  }
  catch (TCLAP::ArgException &e)
  {
    std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
  }
  return 0;
}
