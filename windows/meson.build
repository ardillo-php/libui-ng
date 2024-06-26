# 23 march 2019

windows = import('windows')

libui_sources += [
	'windows/alloc.cpp',
	'windows/area.cpp',
	'windows/areadraw.cpp',
	'windows/areaevents.cpp',
	'windows/areascroll.cpp',
	'windows/areautil.cpp',
	'windows/attrstr.cpp',
	'windows/box.cpp',
	'windows/button.cpp',
	'windows/checkbox.cpp',
	'windows/colorbutton.cpp',
	'windows/colordialog.cpp',
	'windows/combobox.cpp',
	'windows/container.cpp',
	'windows/control.cpp',
	'windows/d2dscratch.cpp',
	'windows/datetimepicker.cpp',
	'windows/debug.cpp',
	'windows/draw.cpp',
	'windows/drawmatrix.cpp',
	'windows/drawpath.cpp',
	'windows/drawtext.cpp',
	'windows/dwrite.cpp',
	'windows/editablecombo.cpp',
	'windows/entry.cpp',
	'windows/events.cpp',
	'windows/fontbutton.cpp',
	'windows/fontdialog.cpp',
	'windows/fontmatch.cpp',
	'windows/form.cpp',
	'windows/graphemes.cpp',
	'windows/grid.cpp',
	'windows/group.cpp',
	'windows/image.cpp',
	'windows/init.cpp',
	'windows/label.cpp',
	'windows/main.cpp',
	'windows/menu.cpp',
	'windows/multilineentry.cpp',
	'windows/opentype.cpp',
	'windows/parent.cpp',
	'windows/progressbar.cpp',
	'windows/radiobuttons.cpp',
	'windows/separator.cpp',
	'windows/sizing.cpp',
	'windows/slider.cpp',
	'windows/spinbox.cpp',
	'windows/stddialogs.cpp',
	'windows/tab.cpp',
	'windows/table.cpp',
	'windows/tabledispinfo.cpp',
	'windows/tabledraw.cpp',
	'windows/tableediting.cpp',
	'windows/tablemetrics.cpp',
	'windows/tabpage.cpp',
	'windows/text.cpp',
	'windows/utf16.cpp',
	'windows/utilwin.cpp',
	'windows/window.cpp',
	'windows/winpublic.cpp',
	'windows/winutil.cpp',
	'windows/webview.cpp',
]

# resources.rc only contains the libui manifest.
# For a DLL, we have to include this directly, so we do so.
# Windows won't link resources in static libraries, so including this would have no effect.
# In those cases, we just need them to include the manifest with the executable (or link it directly into the output executable themselves); they can also customize the manifest as they see fit (assuming nothing breaks in the process).
if libui_mode == 'shared'
	libui_sources += [
		windows.compile_resources('resources.rc',
			args: libui_manifest_args,
			depend_files: ['libui.manifest']),
	]
endif

# TODO prune this list
foreach lib : ['user32', 'kernel32', 'gdi32', 'comctl32', 'uxtheme', 'msimg32', 'comdlg32', 'd2d1', 'dwrite', 'ole32', 'oleaut32', 'oleacc', 'uuid', 'windowscodecs', 'shlwapi', 'advapi32']
	libui_deps += [
		meson.get_compiler('cpp').find_library(lib,
			required: true),
	]
endforeach

if libui_OS == 'windows' and libui_mode == 'shared' and not libui_MSVC
	error('Sorry, but libui for Windows can currently only be built as a static library with MinGW. You will need to either build as a static library or switch to MSVC.')
endif

py3 = import('python3').find_python()

message('Downloading and extracting webview2 nuget package')

arch = target_machine.cpu_family()
if arch not in ['x86_64', 'aarch64']
	error(f'Unexpected architecture @arch@')
endif

webview2_version = '1.0.2592.51'
webview2_hash = '805c79e05184fab18c9fe7b8ba820c598399b97adc1fbf5b0ea490efad91d5b8'
ret = run_command(py3, files('download-webview2.py'), webview2_version, webview2_hash, check: true)
base_path = f'webview2-@webview2_version@' / 'build' / 'native'
inc_dir = include_directories(base_path / 'include')
lib_base_path = meson.current_source_dir() / base_path
lib_path = ''
if arch == 'x86_64'
	lib_path = lib_base_path / 'x64'
else
	lib_path = lib_base_path / 'arm64'
endif

cc = meson.get_compiler('c')
loader_static = cc.find_library('WebView2LoaderStatic', dirs: lib_path)
libui_deps += declare_dependency(include_directories: inc_dir, dependencies: loader_static)