# Project

The goal of this project is to provide an example for how to create/build an application (or a User Mode Driver), which uses the libdxg library https://github.com/microsoft/libdxg and DirectX headers https://github.com/microsoft/DirectX-Headers.

## Getting started

### Directory structure

- /meson.build - Meson build files for inclusion using sub-bproject/wrap.<br>
- /src/main.cpp - source file for the application. <br>
- /subprojects/DirectX-Headers.warp - Meson dependency for DirectX headers <br>
- /subprojects/libdxg.warp - Meson dependency for libdxg library<br>

### Software dependencies

The test application depends on uisng wsl/winadapter.h from https://github.com/microsoft/DirectX-Headers and libdxg library from https://github.com/microsoft/libdxg.

### API references

The WDDM API is described on MSDN https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmthk/

### Ways to consume

- Manually: Just copy the headers somewhere and point your project at them.
- Meson subproject/wrap: Add this entire project as a subproject of your larger project, and use subproject or dependency to consume it.
- Pkg-config: Use Meson to build this project and subprojects, and the resulting installed package can be found via pkg-config.

### Build and Test

The Meson build system is used to compile and build the application and dependencies.

## Contributing

This project welcomes contributions and suggestions.  Most contributions require you to agree to a
Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us
the rights to use your contribution. For details, visit https://cla.opensource.microsoft.com.

When you submit a pull request, a CLA bot will automatically determine whether you need to provide
a CLA and decorate the PR appropriately (e.g., status check, comment). Simply follow the instructions
provided by the bot. You will only need to do this once across all repos using our CLA.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or
contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

## Trademarks

This project may contain trademarks or logos for projects, products, or services. Authorized use of Microsoft
trademarks or logos is subject to and must follow
[Microsoft's Trademark & Brand Guidelines](https://www.microsoft.com/en-us/legal/intellectualproperty/trademarks/usage/general).
Use of Microsoft trademarks or logos in modified versions of this project must not cause confusion or imply Microsoft sponsorship.
Any use of third-party trademarks or logos are subject to those third-party's policies.
