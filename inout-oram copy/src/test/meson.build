applications = include_directories('.')

test_defines = ['-DENABLE_LOGGING']
test_deps = [core_dep, openssl_dep, threadpool_dep, spdlog_dep, util_dep]

if get_option('tracy_profiler')
  message('enabling tracy profiler')

  # if profiler is enabled, load tracy subproject
  tracy = subproject('tracy')
  tracy_dep = tracy.get_variable('tracy_dep')

  plingmap_test_deps += [tracy_dep]
  plingmap_test_defines += [
    '-DTRACY_ENABLE',
  ]
endif

test_sources_alg_tests = [
  'app_test_server.cpp',  # Removed 'test/' prefix
]

test_source_oram = ['app_test_base_oram.cpp']  # Removed 'test/' prefix

test_server_exe = executable(
    'test_server',
    test_sources_alg_tests,
    include_directories: applications,
    dependencies: test_deps,
    c_args: test_defines,
    gnu_symbol_visibility: 'default'
)

test_path_oram_exe = executable (
  'test_pathoram',
  test_source_oram,
  include_directories: applications,
  dependencies: test_deps,
  c_args: test_defines,
  gnu_symbol_visibility: 'default'
)