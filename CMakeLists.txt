cmake_minimum_required(VERSION 3.16)

project(opencv_learn LANGUAGES CXX)

# ---------------------------------------------------------------------------
# 基本设置
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 可执行文件统一输出到 <构建目录>/bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ---------------------------------------------------------------------------
# 定位 OpenCV
#
# 本项目的 OpenCV 就装在 ./opencv 下，这里自动找它的 CMake 配置文件。
# 想用别的 OpenCV，配置时手动传即可：
#   cmake -S . -B build -DOpenCV_DIR="D:/somewhere/opencv/x64/mingw/lib"
# ---------------------------------------------------------------------------
if(NOT OpenCV_DIR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/opencv")
  foreach(_guess
      "${CMAKE_CURRENT_SOURCE_DIR}/opencv/x64/mingw/lib"      # 当前环境就是这种
      "${CMAKE_CURRENT_SOURCE_DIR}/opencv/lib"                # Linux 风格
      "${CMAKE_CURRENT_SOURCE_DIR}/opencv"                    # 顶层兜底
      "${CMAKE_CURRENT_SOURCE_DIR}/opencv/x64/mingw/lib/cmake/opencv4"
      "${CMAKE_CURRENT_SOURCE_DIR}/opencv/lib/cmake/opencv4")
    if(EXISTS "${_guess}/OpenCVConfig.cmake")
      set(OpenCV_DIR "${_guess}" CACHE PATH "OpenCV 的 CMake 配置目录" FORCE)
      break()
    endif()
  endforeach()
endif()

find_package(OpenCV REQUIRED)

message(STATUS "OpenCV version : ${OpenCV_VERSION}")
message(STATUS "OpenCV installed at : ${OpenCV_INSTALL_PATH}")
message(STATUS "OpenCV include dirs : ${OpenCV_INCLUDE_DIRS}")

# ---------------------------------------------------------------------------
# 先找出 OpenCV 的 DLL 在哪，供下面的函数复用
# ---------------------------------------------------------------------------
set(_opencv_dlls "")
if(WIN32)
  set(_opencv_dll_dirs "")
  if(OpenCV_INSTALL_PATH)
    list(APPEND _opencv_dll_dirs
         "${OpenCV_INSTALL_PATH}/x64/mingw/bin"
         "${OpenCV_INSTALL_PATH}/bin"
         "${OpenCV_INSTALL_PATH}/x64/vc16/bin"
         "${OpenCV_INSTALL_PATH}/x64/vc17/bin")
  endif()

  foreach(_dir ${_opencv_dll_dirs})
    if(EXISTS "${_dir}")
      file(GLOB _found_dlls "${_dir}/*.dll")
      if(_found_dlls)
        set(_opencv_dlls "${_found_dlls}")
        message(STATUS "OpenCV DLL source: ${_dir}")
        break()
      endif()
    endif()
  endforeach()
endif()

# ===========================================================================
# 加练习用的函数 —— 一行就够
#
#   add_opencv_demo(目标名 源文件1 [源文件2 ...])
#
# 例：
#   add_opencv_demo(ImageProcess src/ImageProcess_main.cpp src/ImageProcess.cpp)
#
# 一个可执行文件只能有一个 main()。所以：
#   - 带 main() 的那个 .cpp 和它的实现 .cpp 要写在同一行里；
#   - 不要把同一个 .cpp 写进两行，否则会报“multiple definition of main”。
#
# 这个函数会自动链接 OpenCV、加头文件路径、并把 DLL 复制到 exe 旁边。
# ===========================================================================
function(add_opencv_demo target_name)
  add_executable(${target_name} ${ARGN})
  target_link_libraries(${target_name} PRIVATE ${OpenCV_LIBS})
  target_include_directories(${target_name} PRIVATE ${OpenCV_INCLUDE_DIRS})

  # 把项目根目录编进程序，供代码里拼路径用：
  #     std::filesystem::path(APP_ROOT_DIR) / "output"
  # 这样"相对路径"锚定在项目根上，不受运行时的当前工作目录影响
  # （VS Code F5 / 终端 / 双击 exe 三种方式结果一致）。
  target_compile_definitions(${target_name} PRIVATE
    APP_ROOT_DIR="${PROJECT_SOURCE_DIR}")

  if(WIN32 AND _opencv_dlls)
    add_custom_command(TARGET ${target_name} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              ${_opencv_dlls} "$<TARGET_FILE_DIR:${target_name}>"
      COMMENT "Copying OpenCV runtime DLLs")
  endif()
endfunction()

# ---------------------------------------------------------------------------
# 各个练习目标
# ---------------------------------------------------------------------------

# 环境自检示例（读图 -> 灰度 -> Canny 边缘）
add_opencv_demo(${PROJECT_NAME}_demo src/main.cpp)

# 图像处理练习：入口 main() 在 ImageProcess_main.cpp，实现在 ImageProcess.cpp，
# 两者必须编进同一个 exe
add_opencv_demo(ImageProcess src/ImageProcess_main.cpp src/ImageProcess.cpp)

# 新练习照抄上面一行的格式，例如：
# add_opencv_demo(MyFilter src/MyFilter_main.cpp src/MyFilter.cpp)
