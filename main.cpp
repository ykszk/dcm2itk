#include "itkImage.h"
#include "itkGDCMImageIO.h"
#include "itkGDCMSeriesFileNames.h"
#include <itkImageIOFactory.h>
#include "itkImageSeriesReader.h"
#include "itkImageFileWriter.h"
#include <mz.h>
#include <mz_strm.h>
#include <mz_strm_os.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>
#include <filesystem>
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
  ZipReader::ZipReader(char *path)
    : zip_reader(NULL), file_stream(NULL)
  {
    mz_zip_reader_create(&zip_reader);
    mz_stream_os_create(&file_stream);

    err = mz_stream_os_open(file_stream, path, MZ_OPEN_MODE_READ);
    if (err == MZ_OK) {
      err = mz_zip_reader_open(zip_reader, file_stream);
    }
  }

  ZipReader::~ZipReader()
  {
    mz_zip_reader_close(zip_reader);
    mz_stream_os_delete(&file_stream);
    mz_zip_reader_delete(&zip_reader);
  }

};

class TempDir
{
public:
  fs::path path;
  TempDir::TempDir(const fs::path &p)
  {
    path = p;
    fs::create_directories(p);
  }
  TempDir::~TempDir()
  {
    fs::remove_all(path);
  }
  static TempDir New()
  {
    auto base_temp_dir = fs::temp_directory_path();

    for (int i = 0; i < 10000; ++i) {
      {
        auto temp_dir = base_temp_dir / ("tmpzip" + std::to_string(i));
        if (!fs::exists(temp_dir)) {
          return TempDir(temp_dir);
        }
      }
    }
    throw "Failed to find temporary directory";
  }
};
using FileNamesContainer = std::vector<std::string>;

template <typename PixelType, int Dimension>
void _read_n_write(const FileNamesContainer &fileNames, const std::string outFileName, bool compress=true)
{
  using ImageType = itk::Image<PixelType, Dimension>;

  using ReaderType = itk::ImageSeriesReader<ImageType>;
  ReaderType::Pointer reader = ReaderType::New();
  using ImageIOType = itk::GDCMImageIO;
  ImageIOType::Pointer dicomIO = ImageIOType::New();
  reader->SetImageIO(dicomIO);
  reader->SetFileNames(fileNames);
  reader->ForceOrthogonalDirectionOff(); // properly read CTs with gantry tilt

  using WriterType = itk::ImageFileWriter<ImageType>;
  WriterType::Pointer writer = WriterType::New();
  writer->SetFileName(outFileName);
  writer->SetUseCompression(compress);
  writer->SetInput(reader->GetOutput());
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
  /// INT -> SHORT, FLOAT -> FLOAT, DOUBLE -> FLOAT
  constexpr int dim = Dimension;
  switch (componentType) {
  case itk::ImageIOBase::SHORT:
    _read_n_write<int16_t, dim>(fileNames, outFileName);
    return 0;
  case itk::ImageIOBase::INT:
    _read_n_write<int32_t, dim>(fileNames, outFileName);
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

int dir_input(int argc, char * argv[])
{
  if (argc < 2)
  {
    cerr << "Usage: ";
    cerr << argv[0] << " DicomDirectory[.zip]  [outputFileName  [seriesName]]";
    return 1;
  }
  std::string dirName = argv[1];

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
    auto                      seriesItr = seriesUID.begin();
    auto                      seriesEnd = seriesUID.end();

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
    while (seriesItr != seriesUID.end())
    {
      std::string seriesIdentifier;
      if (argc > 3) // If seriesIdentifier given convert only that
      {
        seriesIdentifier = argv[3];
        seriesItr = seriesUID.end();
      }
      else // otherwise convert everything
      {
        seriesIdentifier = seriesItr->c_str();
        seriesItr++;
      }
      cout << "\nReading: ";
      cout << seriesIdentifier << endl;
      FileNamesContainer fileNames = nameGenerator->GetFileNames(seriesIdentifier);
      std::string         outFileName;
      if (argc > 2)
      {
        outFileName = argv[2];
      }
      else
      {
        outFileName = dirName + std::string("/") + seriesIdentifier + ".mha";
      }


      auto imageio = itk::ImageIOFactory::CreateImageIO(fileNames.front().c_str(), itk::ImageIOFactory::FileModeType::ReadMode);
      imageio->SetFileName(fileNames.front());
      imageio->ReadImageInformation();
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
        return read_n_write<2>(fileNames, outFileName, componentType);
      case 3:
        return read_n_write<3>(fileNames, outFileName, componentType);
      }

      return 0;
    }
  }
  catch (itk::ExceptionObject & ex)
  {
    cout << ex << endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

int zipped_input(int argc, char * argv[])
{
  ZipReader reader(argv[1]);
  if (reader.err != MZ_OK) {
    cerr << "MZ error:" << reader.err << endl;
    return 1;
  }
  auto temp_dir = TempDir::New();
  auto temp_dir_str = temp_dir.path.string();
  mz_zip_reader_save_all(reader.zip_reader, temp_dir_str.c_str());
  argv[1] = temp_dir_str.data();
  return dir_input(argc, argv);
}

int main(int argc, char * argv[])
{
  if (argc < 2) {
    return dir_input(argc, argv);
  }
  else {
    if (fs::path(argv[1]).extension() == ".zip") {
      return zipped_input(argc, argv);
    }
    else {
      return dir_input(argc, argv);
    }
  }
}
