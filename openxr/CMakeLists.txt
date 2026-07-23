add_executable(pixel_flow_xr WIN32
  src/main.cpp
  src/palettes.h
  src/shaders.h
)

target_compile_definitions(pixel_flow_xr PRIVATE
  NOMINMAX
  UNICODE
  WIN32_LEAN_AND_MEAN
  XR_USE_GRAPHICS_API_D3D11
  XR_USE_PLATFORM_WIN32
  _UNICODE
)

target_link_libraries(pixel_flow_xr PRIVATE
  openxr_loader
  d3d11
  d3dcompiler
  dxgi
)

if(MSVC)
  target_compile_options(pixel_flow_xr PRIVATE /W4 /permissive- /EHsc /utf-8)
  target_link_options(pixel_flow_xr PRIVATE /SUBSYSTEM:WINDOWS)
endif()

set_target_properties(pixel_flow_xr PROPERTIES
  OUTPUT_NAME "PixelFlowXR"
  VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:pixel_flow_xr>"
)

if(BUILD_TESTING)
  add_executable(pixel_flow_shader_check
    tests/shader_check.cpp
    src/shaders.h
  )
  target_include_directories(pixel_flow_shader_check PRIVATE src)
  target_link_libraries(pixel_flow_shader_check PRIVATE d3dcompiler)
  if(MSVC)
    target_compile_options(pixel_flow_shader_check PRIVATE /W4 /permissive- /EHsc /utf-8)
  endif()
  add_test(NAME pixel_flow_shader_check COMMAND pixel_flow_shader_check)
endif()
