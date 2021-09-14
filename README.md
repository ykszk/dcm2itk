# Simple DICOM to ITK image converter

## Usage
Specify DICOM containing directory
```sh
dcm2itk dcm_dir
```

Specify zipped DICOM directory
```sh
dcm2itk dcm_dir.zip
```

Specify output filename
```sh
dcm2itk dcm_dir output.mha
```

Specify file extension
```sh
dcm2itk dcm_dir --ext .mha
```

## calcsuv
Calculate SUVbwScaleFactor
```
calcsuv pet.dcm --output suv.dcm
```

## BUILD

```bat
rem build zlib
build_deps.bat
rem build itk
build_deps.bat
rem build exe
build.bat
```