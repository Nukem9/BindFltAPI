# bindfltapi

Undocumented BindFlt user mode API otherwise known as `bindfltapi.dll`, `bindflt.dll`, or `bindlink.dll`.

Information has been sourced from [BuildXL](https://github.com/microsoft/BuildXL), [hcsshim](https://github.com/microsoft/hcsshim), and [go-winio](https://github.com/microsoft/go-winio). BindFlt's public successor, [Bindlink](https://learn.microsoft.com/en-us/windows/win32/api/bindlink/), was introduced roughly a year ago but still hasn't been released at the time of writing.

This header expects machines to be running **Windows 10 RS6 or newer**. Older editions have minor API changes and aren't guaranteed to work.

## Building [the example](example/source/main.cpp)

- CMake and vcpkg are expected to be set up beforehand. Visual Studio 2022 is recommended.
- Open the directory in Visual Studio and select the `Debug x64` or `Release x64` preset.
- Build.
- Run `BindfltAPIDemo.exe`.

## License

- [MIT](LICENSE.md)
