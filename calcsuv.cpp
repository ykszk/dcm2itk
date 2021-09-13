#include "itkImage.h"
#include "itkGDCMImageIO.h"
#include "itkGDCMSeriesFileNames.h"
#include <itkImageIOFactory.h>
#include "itkImageSeriesReader.h"
#include "itkImageFileWriter.h"
#include <gdcmReader.h>
#include "gdcmStringFilter.h"
#include <filesystem>
#include <tclap/CmdLine.h>
#include <config.h>
#include <cctype>

struct Args {
  std::string input;
  std::string output;
  std::string outdir;
  std::string tmpdir;
  std::string ext;
};

namespace fs = std::filesystem;
using std::cout;
using std::cerr;
using std::endl;


std::string get_string(const gdcm::DataSet &dataset, const gdcm::Tag &tag) {
  auto elm = dataset.GetDataElement(tag);
  std::string str(elm.GetByteValue()->GetPointer(), elm.GetVL());
  return str;
}

bool ends_with(const std::string& s, const std::string& suffix) {
  if (s.size() < suffix.size()) return false;
  return std::equal(std::rbegin(suffix), std::rend(suffix), std::rbegin(s));
}

void to_valid_filename(std::string &filename)
{
#ifdef _WIN32
  std::string invalids("/:*\"?<>|");
#else
  std::string invalids("/:*\"?<>|");
#endif
  for (int i = 0; i < filename.size(); ++i) {
    auto c = filename[i];
    if (invalids.find(c) != std::string::npos) {
      filename[i] = ' ';
    }
  }
}
std::string rstrip(const std::string s)
{
  auto end_it = s.rbegin();
  while (std::isspace(*end_it))
    ++end_it;
  return std::string(s.begin(), end_it.base());
}



int main(int argc, char * argv[])
{
  Args args;
  args.ext = ".nii.gz"; // default extension

  try {
    TCLAP::CmdLine cmd("Calculate SUV", ' ', PROJECT_VERSION);

    TCLAP::UnlabeledValueArg<std::string> inputDir("input", "Input directory or zip file containing dicom files", true, "", "input");
    cmd.add(inputDir);
    TCLAP::UnlabeledValueArg<std::string> output("output", "(optional) Output filename. Series name (series number if series name is missing) is used by default.", false, "", "output");
    cmd.add(output);
    TCLAP::ValueArg<std::string> outdir("", "outdir", "(optional) Output directory. default: working directory.", false, "", "dirname");
    cmd.add(outdir);
    TCLAP::ValueArg<std::string> tmpdir("", "tmpdir", "(optional) Temporary directory.", false, "", "dirname");
    cmd.add(tmpdir);
    TCLAP::ValueArg<std::string> extArg("e", "ext", "File extension. default: (" + args.ext + ")", false, args.ext, "ext");
    cmd.add(extArg);

    cmd.parse(argc, argv);

  }
  catch (TCLAP::ArgException &e)
  {
    std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
  }
  return 0;
}
