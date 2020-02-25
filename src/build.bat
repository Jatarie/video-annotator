@echo off
cl >nul 2>&1 && (
    REM echo found cl
) || (
    call vcvarsall.bat x64
)
pushd ..\build

REM cl /O2 /FC /F 8000000 /Zi /nologo /I ../avcodec/include /I ../avcodec/include/libavcodec /I ../avcodec/include/libavformat /I ../avcodec/include/libavutil ../src/main.cpp opengl32.lib ../src/glfw3dll.lib ../avcodec/bin/*.lib ../avcodec/bin/avformat.lib
cl /O2 /FC /F 8000000 /Zi /nologo ^
/I ../imgui /I ../avcodec/include /I ../avcodec/include/libavcodec /I ../avcodec/include/libavformat /I ../avcodec/include/libavutil ^
../src/main.cpp opengl32.lib gdi32.lib shell32.lib ../src/glfw3dll.lib ../avcodec/bin/*.lib ../avcodec/bin/avformat.lib

popd
