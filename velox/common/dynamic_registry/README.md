# Dynamic Loading of Velox Extensions

This generic utility adds extensibility features to load User Defined Functions (UDFs), connectors, or types without having to fork and build Velox, through the use of shared libraries.

## Getting started
1. Create a cpp file for your dynamic library
For dynamically loaded function registration, the format followed is mirrored of that of built-in function registration with some noted differences. Using [MyDynamicTestFunction.cpp](tests/MyDynamicTestFunction.cpp) as an example, the function uses the extern "C" keyword to protect against name mangling. A registry() function call is also necessary here. 
Make sure to also include the necessary header file:
    ```
    #include "velox/functions/DynamicUdf.h"
    ```

2. Register functions dynamically by creating .dylib (MacOS) or .so (Linux) shared libraries.
These shared libraries may be made using CMakeLists like the following:

    ```
        add_library(name_of_dynamic_fn SHARED TestFunction.cpp)
        target_link_libraries(name_of_dynamic_fn PRIVATE fmt::fmt glog::glog xsimd)
        target_link_options(name_of_dynamic_fn PRIVATE "-Wl,-undefined,dynamic_lookup")
    ```
    Above, the `fmt::fmt` and `xsimd` libraries are required for all necessary symbols 
    to be defined when loading the `TestFunction.cpp` dynamically. 
    Additionally `glog::glog` is currently required on MacOs. 
    The `target_link_options` allows for symbols to be resolved at runtime on MacOS. 

    On Linux `glog::glog` and the `target_link_options` are optional.