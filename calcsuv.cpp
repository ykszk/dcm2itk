#include <gdcmReader.h>
#include <gdcmImageReader.h>
#include <gdcmWriter.h>
#include <tclap/CmdLine.h>
#include "config.h"
#include "utils.h"

using std::cout;
using std::cerr;
using std::endl;

int main(int argc, char* argv[])
{
  try
  {
    TCLAP::CmdLine cmd("Calculate SUV factor", ' ', PROJECT_VERSION);
    TCLAP::UnlabeledValueArg<std::string> input("input", "Input directory or zip file containing dicom files", true, "", "input", cmd);
    TCLAP::ValueArg<std::string> output("", "output", "(optional) Output dicom with re-calculated rescale-slope.", false, "", "filename", cmd);

    cmd.parse(argc, argv);
    auto input_filename = input.getValue();
    auto output_filename = output.getValue();
    gdcm::Reader reader;
    reader.SetFileName(input_filename.c_str());
    if (!reader.Read())
    {
      cerr << "Could not read: " << input_filename << std::endl;
      return 1;
    }
    auto &dcm = reader.GetFile();
    auto modality = get_string(dcm.GetDataSet(), tags::modality);
    if (modality != "PT") {
      cerr << "Not a PET image." << endl;
      return 1;
    }
    auto SUVbwScaleFactor = calculate_bw_factor(dcm, true);
    if (output.isSet())
    {
      rescale_slope(dcm, SUVbwScaleFactor);
      gdcm::Writer writer;
      writer.SetFileName(output.getValue().c_str());
      writer.SetFile(dcm);
      if (!writer.Write()) {
        cerr << "Could not write: " << input_filename << std::endl;
        return 1;
      };
    }
  }
  catch (TCLAP::ArgException& e)
  {
    std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
    return 1;
  }
  catch (std::exception& e) {
    cerr << e.what() << endl;
    return 1;
  }
  catch (const std::string& e) {
    cerr << e << endl;
    return 1;
  }
  return 0;
}
