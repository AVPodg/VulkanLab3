param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug')
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
$savedPath = $env:PATH
try {
    $tools = Join-Path $PSScriptRoot '.tools'
    if (!(Test-Path -LiteralPath $tools)) { $tools = Join-Path $PSScriptRoot '../lab1/.tools' }
    $portableCmake = Join-Path $tools 'cmake-3.31.6-windows-x86_64/bin/cmake.exe'
    $cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
    if (Test-Path -LiteralPath $portableCmake) { $cmake = $portableCmake }
    elseif ($cmakeCommand) { $cmake = $cmakeCommand.Source }
    else { throw 'CMake 3.24+ is required.' }
    $options = @()
    # Reuse downloaded source libraries, but compile everything into lab2/build-local.
    $oldDeps = Join-Path $PSScriptRoot '../lab1/build-local/_deps'
    foreach ($dependency in @('glfw', 'vk-bootstrap', 'vulkanmemoryallocator', 'imgui', 'glm')) {
        $source = Join-Path $oldDeps "$dependency-src"
        if (Test-Path -LiteralPath "$source/CMakeLists.txt") {
            $options += "-DFETCHCONTENT_SOURCE_DIR_$($dependency.ToUpper())=$source"
        } elseif ($dependency -eq 'imgui' -and (Test-Path -LiteralPath "$source/imgui.h")) {
            $options += "-DFETCHCONTENT_SOURCE_DIR_IMGUI=$source"
        }
    }
    $compiler = 'C:/msys64/ucrt64/bin'
    if (Test-Path -LiteralPath "$compiler/g++.exe") {
        $env:PATH = "$compiler;$env:PATH"
        $generator = 'MinGW Makefiles'
        $build = 'build-local'
        $sdk = Join-Path $tools 'ucrt64'
        if (Test-Path -LiteralPath "$sdk/bin/glslc.exe") {
            $env:PATH = "$sdk/bin;$env:PATH"
            $options += @("-DVulkan_INCLUDE_DIR=$sdk/include", "-DVulkan_LIBRARY=$sdk/lib/libvulkan-1.dll.a", "-DLAB_GLSLC=$sdk/bin/glslc.exe")
        }
    } else {
        $generator = 'Visual Studio 17 2022'
        $build = 'build-msvc'
        $options += @('-A', 'x64')
    }
    & $cmake -S . -B $build -G $generator "-DCMAKE_BUILD_TYPE=$Configuration" @options
    if ($LASTEXITCODE) { throw 'CMake configuration failed.' }
    & $cmake --build $build --config $Configuration --parallel 4
    if ($LASTEXITCODE) { throw 'Build failed.' }
    & $cmake --install $build --config $Configuration --prefix (Join-Path $PSScriptRoot 'bin')
    if ($LASTEXITCODE) { throw 'Copying runnable files failed.' }
    Write-Host 'Build complete. Run .\bin\camera-lighting-lab.exe'
} finally { $env:PATH = $savedPath }
