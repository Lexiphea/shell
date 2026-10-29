# Header only fzf matcher with weighted fields and frecency from vicinae (GPL-3.0)
# https://github.com/vicinaehq/vicinae/tree/b94c510/src/lib/fuzzy
add_library(vicinae-fuzzy INTERFACE)
target_include_directories(vicinae-fuzzy SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(vicinae-fuzzy INTERFACE cxx_std_23)
