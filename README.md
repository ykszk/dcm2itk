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

Specify file extension
```sh
dcm2itk dcm_dir --ext .mha
```


## BUILD

```bat
rem build zlib
build_deps.bat
rem build itk
build_deps.bat
```