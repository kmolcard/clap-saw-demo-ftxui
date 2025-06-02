# clap-saw-demo-ftxui

This project is a port of the [clap-saw-demo](https://github.com/surge-synthesizer/clap-saw-demo)
VSTGUI example to have the same clap engine but use [FTXUI](https://github.com/ArthurSonzogni/FTXUI)
as the renderer for the clap gui. Currently it works on macOS with support for other platforms planned.

The project works by using the [clap-ftxui-support](https://github.com/kmolcard/clap-ftxui-support)
library which provides an interface between the FTXUI terminal-based rendering and the clap 
gui interface.

That library provides a terminal-based UI framework that creates native windows with embedded 
terminal interfaces for plugin GUIs. This approach offers a lightweight, modern, and efficient
alternative to traditional GUI frameworks while maintaining full CLAP compatibility.

To use FTXUI, make an editor class which subclasses `ftxui_clap_editor` such as 
[the ClapSawDemoEditor here](src/clap-saw-demo-editor.h#L14)
then implement the various mechanics to connect and render as shown in the cpp file.

## Features

- **Terminal-based UI**: Modern, efficient terminal-style interface
- **Full CLAP compatibility**: Supports all standard CLAP plugin features
- **Lightweight**: Minimal resource usage compared to traditional GUI frameworks
- **Component-based**: Uses FTXUI's component architecture for clean UI design
- **Cross-platform**: Designed for macOS, Linux, and Windows support 


# Building the Example

Our CI pipeline shows the minimal build for all platforms:

```shell
git clone --recurse-submodules https://github.com/kmolcard/clap-saw-demo-ftxui
cd clap-saw-demo-ftxui
cmake -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

To build, you will need:
- **macOS**: Xcode and CMake
- **Linux**: GCC/Clang and CMake  
- **Windows**: Visual Studio and CMake

## Testing

The project includes several test utilities to verify functionality:

```shell
# Test basic plugin loading
./build/test_plugin

# Test GUI functionality  
./build/test_gui

# Test parameter functionality
./build/test_parameters
```

## IDE Integration

As with all cmake projects you can integrate with your IDE of choice. For instance
to use Xcode directly:

```shell
cmake -B build -G Xcode
open build/clap-saw-demo-ftxui.xcodeproj
```

## Plugin Installation

On macOS, the plugin will be built as `clap-saw-demo-ftxui.clap` and can be installed to:
```
~/Library/Audio/Plug-Ins/CLAP/
```

