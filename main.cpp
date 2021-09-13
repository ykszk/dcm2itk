#include "itkImage.h"
#include "itkGDCMImageIO.h"
#include "itkGDCMSeriesFileNames.h"
#include <itkImageIOFactory.h>
#include "itkImageSeriesReader.h"
#include "itkImageFileWriter.h"
#include <gdcmReader.h>
#include "gdcmStringFilter.h"
#include <mz.h>
#include <mz_strm.h>
#include <mz_strm_os.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>
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


class ZipReader
{
public:
  void* zip_reader;
  void* file_stream;
  int32_t err;
  ZipReader(const char *path)
    : zip_reader(NULL), file_stream(NULL)
  {
    mz_zip_reader_create(&zip_reader);
    mz_stream_os_create(&file_stream);

    err = mz_stream_os_open(file_stream, path, MZ_OPEN_MODE_READ);
    if (err == MZ_OK) {
      err = mz_zip_reader_open(zip_reader, file_stream);
    }
  }

  ~ZipReader()
  {
    mz_zip_reader_close(zip_reader);
    mz_stream_os_delete(&file_stream);
    mz_zip_reader_delete(&zip_reader);
  }
};

fs::path get_available_name(const fs::path &dir, const std::string &stem, const std::string &ext)
{
  for (int i = 0; i < 10000; ++i) {
    {
      auto temp_dir = dir / (stem + "_(" + std::to_string(i) + ")" + ext);
      if (!fs::exists(temp_dir)) {
        return temp_dir;
      }
    }
  }
  throw "Could not find available filename";
}

class TempDir
{
public:
  fs::path path;
  TempDir(const fs::path &p)
  {
    path = p;
    fs::create_directories(p);
  }
  ~TempDir()
  {
    fs::remove_all(path);
  }
  static TempDir New()
  {
    auto base_temp_dir = fs::temp_directory_path();
    return TempDir(get_available_name(base_temp_dir, "tmpzip", ""));
  }
  static TempDir New(const std::string &tmpdir)
  {
    return TempDir(get_available_name(tmpdir, "tmpzip", ""));
  }
};
using FileNamesContainer = std::vector<std::string>;

std::string get_string(const gdcm::DataSet &dataset, const gdcm::Tag &tag) {
  auto elm = dataset.GetDataElement(tag);
  std::string str(elm.GetByteValue()->GetPointer(), elm.GetVL());
  return str;
}
template <typename PixelType, int Dimension>
void _read_n_write(const FileNamesContainer &fileNames, const std::string outFileName, bool compress = true)
{
//  auto imageio = itk::ImageIOFactory::CreateImageIO(fileNames.front().c_str(), itk::ImageIOFactory::FileModeType::ReadMode);
  auto imageio = itk::GDCMImageIO::New();
  imageio->LoadPrivateTagsDefaultOn();
  imageio->LoadPrivateTagsOn();
  imageio->SetFileName(fileNames.front());
  imageio->ReadImageInformation();

  auto &meta = imageio->GetMetaDataDictionary();
  auto modality = get_value(meta, "0008|0060");
  auto units = get_value(meta, "0054|1001");
  using ImageType = itk::Image<PixelType, Dimension>;

  using ReaderType = itk::ImageSeriesReader<ImageType>;
  typename ReaderType::Pointer reader = ReaderType::New();
  using ImageIOType = itk::GDCMImageIO;
  ImageIOType::Pointer dicomIO = ImageIOType::New();
  reader->SetImageIO(dicomIO);
  reader->SetFileNames(fileNames);
  reader->ForceOrthogonalDirectionOff(); // properly read CTs with gantry tilt

  auto image = reader->GetOutput();
  if (modality == "PT") {
    for (auto& filename : fileNames) {
      {
      }
    }
  }
  return;

  using WriterType = itk::ImageFileWriter<ImageType>;
  typename WriterType::Pointer writer = WriterType::New();
  writer->SetFileName(outFileName);
  writer->SetUseCompression(compress);
  writer->SetInput(image);
  cout << "Writing: " << outFileName << endl;
  try
  {
    writer->Update();
  }
  catch (itk::ExceptionObject & ex)
  {
    cerr << ex << endl;
  }
}

template <int Dimension>
int read_n_write(const FileNamesContainer &fileNames, const std::string outFileName, itk::ImageIOBase::IOComponentType componentType)
{
  /// UINT8 -> UINT8, SHORT -> SHORT, INT -> SHORT, FLOAT -> FLOAT, DOUBLE -> FLOAT
  constexpr int dim = Dimension;
  switch (componentType) {
  case itk::ImageIOBase::UCHAR:
    _read_n_write<uint8_t, dim>(fileNames, outFileName);
    return 0;
  case itk::ImageIOBase::SHORT:
    _read_n_write<int16_t, dim>(fileNames, outFileName);
    return 0;
  case itk::ImageIOBase::INT:
    _read_n_write<int16_t, dim>(fileNames, outFileName);
    return 0;
  case itk::ImageIOBase::FLOAT:
    _read_n_write<float, dim>(fileNames, outFileName, false);
    return 0;
  case itk::ImageIOBase::DOUBLE:
    _read_n_write<float, dim>(fileNames, outFileName, false);
    return 0;
  default:
    cerr << "Unsupported component type:" << itk::ImageIOBase::GetComponentTypeAsString(componentType) << endl;
    return 1;
  }
}

template <typename ValueType = std::string>
const ValueType& get_value(const itk::MetaDataDictionary &meta, const std::string &key)
{
  auto ptr = dynamic_cast<const itk::MetaDataObject<ValueType> *>(meta.Get(key));
  return ptr->GetMetaDataObjectValue();
}
bool has_value(const itk::MetaDataDictionary &meta, const std::string &key)
{
  return meta.HasKey(key) && get_value(meta, key) != "";
}

bool ends_with(const std::string& s, const std::string& suffix) {
  if (s.size() < suffix.size()) return false;
  return std::equal(std::rbegin(suffix), std::rend(suffix), std::rbegin(s));
}

void to_valid_filename(std::string &filename)
{
#ifdef _WIN32
  std::string invalids("/\:*\"?<>|");
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

int dir_input(const Args &args)
{
  std::string dirName = args.input;

  using NamesGeneratorType = itk::GDCMSeriesFileNames;
  NamesGeneratorType::Pointer nameGenerator = NamesGeneratorType::New();

  nameGenerator->SetUseSeriesDetails(true);
  nameGenerator->SetRecursive(true);
  nameGenerator->AddSeriesRestriction("0008|0021");
  nameGenerator->SetGlobalWarningDisplay(false);
  nameGenerator->SetDirectory(dirName);

  try
  {
    using SeriesIdContainer = std::vector<std::string>;
    const SeriesIdContainer & seriesUID = nameGenerator->GetSeriesUIDs();
    auto seriesItr = seriesUID.begin();
    auto seriesEnd = seriesUID.end();

    if (seriesItr != seriesEnd)
    {
      cout << "The directory: ";
      cout << dirName << endl;
      cout << "Contains the following DICOM Series: ";
      cout << endl;
    }
    else
    {
      cout << "No DICOMs in: " << dirName << endl;
      return EXIT_SUCCESS;
    }

    while (seriesItr != seriesEnd)
    {
      cout << seriesItr->c_str() << endl;
      ++seriesItr;
    }

    seriesItr = seriesUID.begin();
    int series_count = 0;
    while (seriesItr != seriesUID.end())
    {
      std::string seriesIdentifier = seriesItr->c_str();
      seriesItr++;
      series_count++;
      cout << "Reading: " << seriesIdentifier << endl;
      FileNamesContainer fileNames = nameGenerator->GetFileNames(seriesIdentifier);

      auto imageio = itk::ImageIOFactory::CreateImageIO(fileNames.front().c_str(), itk::ImageIOFactory::FileModeType::ReadMode);
      imageio->SetFileName(fileNames.front());
      imageio->ReadImageInformation();
      auto &meta = imageio->GetMetaDataDictionary();

      std::string outFileName;
      if (args.output != "")
      {
        if (series_count == 1) {
          outFileName = args.output;
        }
        else {
          fs::path output_path(args.output);
          auto stem = output_path.stem().string();
          auto ext = output_path.extension().string();
          if (ends_with(args.output, ".nii.gz")) {
            ext = ".nii.gz";
            stem = std::string(args.output.c_str(), args.output.size() - 7);
          }
          outFileName = (output_path.parent_path() / (stem + "_(" + std::to_string(series_count) + ")" + ext)).string();
        }
      }
      else
      {
        std::string stem(seriesIdentifier);
        if (has_value(meta, "0008|103e")) { // series description
          stem = get_value(meta, "0008|103e");
        }
        else if (has_value(meta, "0020|0011")) { // series number
          stem = get_value(meta, "0020|0011");
        }
        to_valid_filename(stem);
        stem = rstrip(stem);
        outFileName = (fs::path(args.outdir) / (stem + args.ext)).string();
        if (fs::exists(outFileName)) {
          outFileName = get_available_name(fs::path(args.outdir), stem, args.ext).string();
        }
      }

      auto dimension = imageio->GetNumberOfDimensions();
      auto componentType = imageio->GetComponentType();
      auto pixelType = imageio->GetPixelType();
      auto numberOfComponents = imageio->GetNumberOfComponents();
      if (dimension != 2 && dimension != 3) {
        cerr << "Invalid image dimension:" << dimension;
        return 1;
      }
      if (pixelType != itk::ImageIOBase::SCALAR) {
        cerr << "Invalid pixel type:" << itk::ImageIOBase::GetPixelTypeAsString(pixelType) << endl;
        return 1;
      }
      using IOBase = itk::ImageIOBase;

      if (numberOfComponents != 1) {
        cerr << "Invalid num of components:" << numberOfComponents << endl;
      }
      switch (dimension) {
      case 2:
        read_n_write<2>(fileNames, outFileName, componentType);
        break;
      case 3:
        read_n_write<3>(fileNames, outFileName, componentType);
        break;
      }
    }
  }
  catch (itk::ExceptionObject & ex)
  {
    cout << ex << endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

int zipped_input(const Args &args)
{
  ZipReader reader(args.input.c_str());
  if (reader.err != MZ_OK) {
    cerr << "MZ error:" << reader.err << endl;
    return EXIT_FAILURE;
  }
  auto temp_dir = TempDir::New(args.tmpdir);
  auto temp_dir_str = temp_dir.path.string();
  mz_zip_reader_save_all(reader.zip_reader, temp_dir_str.c_str());
  Args new_args(args);
  new_args.input = temp_dir_str;
  return dir_input(new_args);
}

int main(int argc, char * argv[])
{
  Args args;
  args.ext = ".nii.gz"; // default extension

  try {
    TCLAP::CmdLine cmd("Simple DICOM to ITK image converter", ' ', PROJECT_VERSION);

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

    args.input = inputDir.getValue();
    if (output.isSet()) {
      args.output = output.getValue();
      args.outdir = fs::path(args.output).parent_path().string();
      if (outdir.isSet()) {
        cout << "Warning: <outdir>=<" << outdir.getValue() << "> is ignored." << endl;
      }
    }
    else {
      if (outdir.isSet()) {
        args.outdir = outdir.getValue();
      }
      else {
        args.outdir = fs::path(args.input).parent_path().string();
      }
    }
    args.ext = extArg.getValue();
    if (tmpdir.isSet()) {
        args.tmpdir = tmpdir.getValue();
    }
    else {
      args.tmpdir = "";
    }


    if (!fs::exists(args.input)) {
      cerr << "Fatal error: Could not find input(" << args.input << ")." << endl;
      return EXIT_FAILURE;
    }
    if (args.outdir!="" && !fs::exists(args.outdir)) {
      cerr << "Fatal error: Could not find outdir(" << args.outdir << ")." << endl;
      return EXIT_FAILURE;
    }

    if (fs::path(args.input).extension() == ".zip") {
      return zipped_input(args);
    }
    else {
      return dir_input(args);
    }
  }
  catch (TCLAP::ArgException &e)
  {
    std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
  }
  return 0;
}
