project('io-oram',
  ['c', 'cpp'],
  version : '0.1',
  default_options : [
    'warning_level=3',
    'cpp_std=c++20',
  ],
)

# set c/cpp arguments
cc_warning_flags = [
  '-Wno-unused-parameter',
  '-Wno-unused-variable',
  '-Wno-unused-private-field',
]
add_global_arguments(cc_warning_flags, language : 'c')
add_global_arguments(cc_warning_flags, language : 'cpp')

# check compilers
cc = meson.get_compiler('c')
cxx = meson.get_compiler('cpp')

oe_crypto_lib = 'openssl'
openssl_dep = dependency('openssl', version: '>=1.1.0')

# ensure that the c/c++ compiler is clang
if cc.get_id() == 'clang' and cxx.get_id() == 'clang'
  # log the compiler id and version
  # message('Using clang compiler')
  message('using c compiler: @0@ @1@'.format(cc.get_id(), cc.version()))
  message('using c++ compiler: @0@ @1@'.format(cxx.get_id(), cxx.version()))
else
  error('This project requires the clang compiler')
endif

thread_dep = dependency('threads')

spdlog_dep = dependency('spdlog', required : true)

tomlpp_dep = declare_dependency(
  include_directories: include_directories('thirdparty/external/tomlplusplus')
)

# common
subdir('threadpool')
subdir('core')
subdir('utils')

# Add SEAL library
seal_inc = include_directories('seal')
seal_lib = static_library('seal', 
  [], # Empty sources array since we're using header-only implementation
  include_directories: [seal_inc, include_directories('.'), include_directories('utils')],
  dependencies: [openssl_dep, thread_dep, spdlog_dep, core_dep, threadpool_dep, util_dep]
)

seal_dep = declare_dependency(
  include_directories: seal_inc,
  link_with: seal_lib
)

# Create a subdirectory for SEAL tests
seal_test_exe = executable('test_seal',
  'seal/test/test_seal.cpp',
  include_directories: [include_directories('.')],
  dependencies: [seal_dep, openssl_dep, thread_dep, spdlog_dep, core_dep, threadpool_dep, util_dep],
  cpp_args: ['-DENABLE_LOGGING'] + cc_warning_flags,
  gnu_symbol_visibility: 'default'
)

# test
subdir('test')
