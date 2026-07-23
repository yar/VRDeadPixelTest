add_executable(vr_dead_pixel_test WIN32
  src/main.cpp
  src/palettes.h
  src/shaders.h
)

target_compile_definitions(vr_dead_pixel_test PRIVATE
  NOMINMAX
  UNICODE
  WIN32_LEAN_AND_MEAN
  XR_USE_GRAPHICS_API_D3D11
  XR_USE_PLATFORM_WIN32
  _UNICODE
)

target_link_libraries(vr_dead_pixel_test PRIVATE
  openxr_loader
  d3d11
  d3dcompiler
  dxgi
)

if(MSVC)
  target_compile_options(vr_dead_pixel_test PRIVATE /W4 /permissive- /EHsc /utf-8)
  target_link_options(vr_dead_pixel_test PRIVATE /SUBSYSTEM:WINDOWS)
endif()

set_target_properties(vr_dead_pixel_test PROPERTIES
  OUTPUT_NAME "VRDeadPixelTest"
  VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:vr_dead_pixel_test>"
)

if(BUILD_TESTING)
  add_executable(vr_dead_pixel_test_shader_check
    tests/shader_check.cpp
    src/shaders.h
  )
  target_include_directories(vr_dead_pixel_test_shader_check PRIVATE src)
  target_link_libraries(vr_dead_pixel_test_shader_check PRIVATE d3dcompiler)
  if(MSVC)
    target_compile_options(vr_dead_pixel_test_shader_check PRIVATE /W4 /permissive- /EHsc /utf-8)
  endif()
  add_test(NAME vr_dead_pixel_test_shader_check COMMAND vr_dead_pixel_test_shader_check)
endif()
