project(
  'bmcutility',
  'cpp',
  version: '0.1',
  meson_version: '>=0.58.0',
  default_options: [
    'warning_level=3',
    'cpp_std=c++23',
  ])


add_project_arguments('-DSSL_ON', language : 'cpp')

openssl_dep = dependency('openssl', version: '>=1.1.1')
boost_dep = dependency('boost',modules: ['coroutine','url'])
json_dep = dependency('nlohmann_json')
#gtest = subproject('gtest')
#chaibeast=subproject('chaibeast')
reactor=subproject('reactor')
#chaibeast_dep=chaibeast.get_variable('chaibeast_dep')
reactor_dep=reactor.get_variable('reactor_dep')

core_includes = include_directories(['./include'])

subdir('aggregator')
subdir('filesync')
subdir('reverseproxy')
subdir('credcopier')
subdir('httpserver')




